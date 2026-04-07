import os
import random
import subprocess
from dataclasses import dataclass

# =========================
# CONFIGURATION
# =========================

N_RUNS = 5
EXECUTABLE_PATH = r"C:\Users\filip\CLionProjects\unipd-or2-tiberio-serain\cmake-build-debug---vsc\unipd_or2_tiberio_serain.exe"  # <-- change this
OUTPUT_ROOT = "runs"

# =========================
# MODEL CLASS
# =========================


@dataclass
class Model:
    name: str
    file: str
    time_limit: float
    model_type: int
    nodes_count: int = None
    no_plot: bool = True

    def build_command(self, seed: int, output_file: str):
        cmd = [
            EXECUTABLE_PATH,
            "-f" if self.file else "",
            self.file if self.file else "",
            "--out",
            output_file,
            "--time_limit",
            str(self.time_limit),
            "-model",
            str(self.model_type),
            "-seed",
            str(seed),
        ]

        if self.nodes_count is not None:
            cmd += ["-nodes_count", str(self.nodes_count)]

        if self.no_plot:
            cmd.append("--no-plot")

        return cmd


# =========================
# DEFINE MODELS
# =========================

MODELS = [
    Model(
        name="model_A",
        time_limit=60,
        model_type=0,
    ),
    Model(
        name="model_B",
        time_limit=60,
        model_type=1,
    ),
]


# =========================
# MAIN LOOP
# =========================


def main():
    os.makedirs(OUTPUT_ROOT, exist_ok=True)

    for run_idx in range(N_RUNS):
        seed = random.randint(0, 10**9)

        run_folder = os.path.join(OUTPUT_ROOT, f"run_{seed}")
        os.makedirs(run_folder, exist_ok=True)

        print(f"\n=== RUN {run_idx+1}/{N_RUNS} | seed={seed} ===")

        for model in MODELS:
            output_file = os.path.join(run_folder, f"{model.name}.txt")

            cmd = model.build_command(seed, output_file)

            print(f"Running {model.name}...")
            print(" ".join(cmd))

            try:
                subprocess.run(cmd, check=True)
            except subprocess.CalledProcessError as e:
                print(f"Error running {model.name}: {e}")


if __name__ == "__main__":
    main()
