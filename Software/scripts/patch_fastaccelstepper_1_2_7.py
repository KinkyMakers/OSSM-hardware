Import("env")

from pathlib import Path


libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
library_dir = libdeps_dir / "FastAccelStepper"
manifest = library_dir / "library.properties"
source = library_dir / "src" / "FastAccelStepper.cpp"

if not manifest.exists() or not source.exists():
    raise RuntimeError(
        "FastAccelStepper must be installed before applying the 1.2.7 timed-queue patch"
    )

version_lines = {
    key.strip(): value.strip()
    for key, value in (
        line.split("=", 1)
        for line in manifest.read_text().splitlines()
        if "=" in line
    )
}
version = version_lines.get("version")
if version != "1.2.7":
    raise RuntimeError(
        f"Timed-queue compatibility patch requires FastAccelStepper 1.2.7, found {version}"
    )

marker = "// OSSM: permit moveTimed() to resume after forceStop()."
needle = """MoveTimedResultCode FastAccelStepper::moveTimed(int16_t steps,
                                                uint32_t duration,
                                                uint32_t* actual_duration,
                                                bool start) {
"""
replacement = needle + f"""  {marker}
  // forceStop() deliberately blocks subsequent queue writes, and ordinary
  // ramp moves clear that block in fill_queue(). Timed moves bypass the ramp,
  // so clear it only after the old queue is fully stopped and empty.
  StepperQueue* timed_queue = _queue();
  if (!timed_queue->isRunning() && isQueueEmpty()) {{
    timed_queue->ignore_commands = false;
  }}
"""

contents = source.read_text()
if marker not in contents:
    if needle not in contents:
        raise RuntimeError(
            "FastAccelStepper 1.2.7 moveTimed() source did not match the audited patch target"
        )
    source.write_text(contents.replace(needle, replacement, 1))
    print("Applied FastAccelStepper 1.2.7 timed-queue resume patch")
