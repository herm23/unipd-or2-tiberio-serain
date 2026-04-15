import os
import random
import subprocess
from dataclasses import dataclass

# =========================
# CONFIGURATION
# =========================

N_RUNS = 20
EXECUTABLE_PATH = r"C:\Users\filip\CLionProjects\unipd-or2-tiberio-serain\cmake-build-debug---vsc\unipd_or2_tiberio_serain.exe"
OUTPUT_ROOT = "runs"

NODE_SIZES = [50, 100, 300, 1000]

# =========================
# MODEL CLASS
# =========================


@dataclass
class Model:
    name: str
    time_limit: float
    model_type: int
    nodes_count: int
    verbosity: int = 0
    no_plot: bool = True

    def build_command(self, seed: int, output_file: str):
        cmd = [
            EXECUTABLE_PATH,
            "-seed",
            str(seed),
            "-nodes_count",
            str(self.nodes_count),
            "-time_limit",
            str(self.time_limit),
            "-v",
            str(self.verbosity),
            "-model_type",
            str(self.model_type),
            "--out",
            output_file,
        ]

        if self.no_plot:
            cmd.append("--no-plot")

        return cmd


# =========================
# MAIN LOOP
# =========================


def main():
    os.makedirs(OUTPUT_ROOT, exist_ok=True)

    for nodes in NODE_SIZES:
        print(f"\n===== NODES: {nodes} =====")

        # definisci i modelli per questo numero di nodi
        models = [
            Model(name="NN_2OPT_MS", time_limit=60, model_type=1, nodes_count=nodes),
            Model(name="NN_VNS", time_limit=60, model_type=3, nodes_count=nodes),
            Model(name="NN_TABU", time_limit=60, model_type=4, nodes_count=nodes),
        ]

        for run_idx in range(N_RUNS):
            seed = random.randint(0, 10**9)

            print(f"\n--- RUN {run_idx+1}/{N_RUNS} | seed={seed} ---")

            for model in models:
                # struttura cartelle: nodes/model/seed/
                output_dir = os.path.join(
                    OUTPUT_ROOT, str(nodes), model.name, f"seed_{seed}"
                )
                os.makedirs(output_dir, exist_ok=True)

                # filename = serializzazione parametri
                filename = f"nodes{nodes}_model{model.model_type}_seed{seed}_tl{model.time_limit}.txt"
                output_file = os.path.join(output_dir, filename)

                cmd = model.build_command(seed, output_file)

                print(f"Running {model.name}...")
                print(" ".join(cmd))

                try:
                    subprocess.run(cmd, check=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error running {model.name}: {e}")


if __name__ == "__main__":
    main()
