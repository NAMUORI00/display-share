use std::thread::{self, JoinHandle};

use capture_core::{
    AnalysisFrame, CaptureRoi, DetectionResult, HsvMaskStats, HsvTuneResult, InferenceBackend,
    InferenceDiagnostics, InferenceSettings, ProviderState, Size2D,
};
use crossbeam_channel::{Receiver, Sender, TrySendError, bounded};
use inference_dml::DirectMlInferenceBackend;
use pipeline::{FramePipeline, FramePipelineSettings};

enum WorkerCommand {
    Process {
        analysis: AnalysisFrame,
        settings: FramePipelineSettings,
    },
    LoadModel(InferenceSettings),
    Shutdown,
}

#[allow(clippy::large_enum_variant)]
pub(crate) enum WorkerEvent {
    Frame {
        hsv: HsvMaskStats,
        hsv_tune: Option<HsvTuneResult>,
        yolo_detections: Vec<DetectionResult>,
        capture_roi: CaptureRoi,
        capture_size: Size2D,
        model_input: Size2D,
        processing_ms: f64,
        provider_state: ProviderState,
    },
    ModelReady {
        state: ProviderState,
        diagnostics: Box<InferenceDiagnostics>,
        summary: String,
    },
    Failed(String),
}

pub(crate) struct VisionWorker {
    cmd_tx: Sender<WorkerCommand>,
    event_rx: Receiver<WorkerEvent>,
    handle: Option<JoinHandle<()>>,
}

impl VisionWorker {
    pub(crate) fn spawn() -> Self {
        let (cmd_tx, cmd_rx) = bounded::<WorkerCommand>(2);
        let (event_tx, event_rx) = bounded::<WorkerEvent>(4);
        let handle = thread::spawn(move || worker_loop(cmd_rx, event_tx));
        Self {
            cmd_tx,
            event_rx,
            handle: Some(handle),
        }
    }

    pub(crate) fn try_submit(
        &self,
        analysis: AnalysisFrame,
        settings: FramePipelineSettings,
    ) -> bool {
        match self
            .cmd_tx
            .try_send(WorkerCommand::Process { analysis, settings })
        {
            Ok(()) => true,
            Err(TrySendError::Full(_) | TrySendError::Disconnected(_)) => false,
        }
    }

    pub(crate) fn load_model(&self, settings: InferenceSettings) {
        let _ = self.cmd_tx.send(WorkerCommand::LoadModel(settings));
    }

    pub(crate) fn try_recv_event(&self) -> Option<WorkerEvent> {
        self.event_rx.try_recv().ok()
    }
}

impl Drop for VisionWorker {
    fn drop(&mut self) {
        let _ = self.cmd_tx.send(WorkerCommand::Shutdown);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}

fn worker_loop(cmd_rx: Receiver<WorkerCommand>, event_tx: Sender<WorkerEvent>) {
    let pipeline = FramePipeline::new();
    let mut inference = DirectMlInferenceBackend::default();

    while let Ok(command) = cmd_rx.recv() {
        match command {
            WorkerCommand::Shutdown => break,
            WorkerCommand::LoadModel(settings) => match inference.initialize(settings) {
                Ok(state) => {
                    let diagnostics = inference.diagnostics();
                    let summary = inference_summary(&diagnostics);
                    let _ = event_tx.send(WorkerEvent::ModelReady {
                        state,
                        diagnostics: Box::new(diagnostics),
                        summary,
                    });
                }
                Err(err) => {
                    let _ = event_tx.send(WorkerEvent::Failed(err.to_string()));
                }
            },
            WorkerCommand::Process {
                analysis,
                mut settings,
            } => {
                // Preview is produced on the capture thread; never request GPU readback here.
                settings.want_preview_buffer = false;
                settings.preview_enabled = false;

                let started = std::time::Instant::now();
                match pipeline.process_analysis_roi(&analysis, &settings, Some(&mut inference)) {
                    Ok(report) => {
                        let processing_ms = started.elapsed().as_secs_f64() * 1000.0;
                        let _ = event_tx.send(WorkerEvent::Frame {
                            hsv: report.hsv,
                            hsv_tune: report.hsv_tune,
                            yolo_detections: report.yolo_detections,
                            capture_roi: report.capture_roi,
                            capture_size: report.capture_size,
                            model_input: report.model_input,
                            processing_ms,
                            provider_state: inference.provider_state(),
                        });
                    }
                    Err(err) => {
                        let _ = event_tx.send(WorkerEvent::Failed(err.to_string()));
                    }
                }
            }
        }
    }
}

fn inference_summary(diagnostics: &InferenceDiagnostics) -> String {
    let mut parts = vec![format!("classes={}", diagnostics.class_count)];
    if let Some(shape) = &diagnostics.last_output_shape {
        parts.push(format!("output={shape:?}"));
    }
    if let Some(graph) = &diagnostics.model_metadata.graph_name {
        parts.push(format!("graph={graph}"));
    }
    parts.join(", ")
}
