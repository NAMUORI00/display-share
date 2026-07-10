mod app;
mod image_presenter;
mod vision_worker;

fn main() -> anyhow::Result<()> {
    app::run()
}
