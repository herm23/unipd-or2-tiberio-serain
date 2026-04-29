import os
import random
import subprocess
import concurrent.futures
from dataclasses import dataclass

# =========================
# CONFIGURATION
# =========================

N_RUNS = 30
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


def run_model(model: Model, seed: int, run_idx: int):
    # struttura cartelle: nodes/model/seed/
    output_dir = os.path.join(
        OUTPUT_ROOT, str(model.nodes_count), model.name, f"seed_{seed}"
    )
    os.makedirs(output_dir, exist_ok=True)

    # filename = serializzazione parametri
    filename = f"nodes{model.nodes_count}_model{model.model_type}_seed{seed}_tl{model.time_limit}.txt"
    output_file = os.path.join(output_dir, filename)

    cmd = model.build_command(seed, output_file)

    print(f"Running {model.name} (Nodes: {model.nodes_count}, Run: {run_idx+1}, Seed: {seed})...")
    # print(" ".join(cmd)) # redundant in parallel mode

    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        return f"Finished: {model.name} nodes={model.nodes_count} seed={seed}"
    except subprocess.CalledProcessError as e:
        return f"Error running {model.name} nodes={model.nodes_count} seed={seed}: {e.stderr.decode()}"


# =========================
# MAIN LOOP
# =========================


def main():
    os.makedirs(OUTPUT_ROOT, exist_ok=True)

    tasks = []
    for nodes in NODE_SIZES:
        # definisci i modelli per questo numero di nodi
        models = [
            Model(name="NN_2OPT_MS", time_limit=300, model_type=1, nodes_count=nodes),
            Model(name="NN_VNS", time_limit=300, model_type=3, nodes_count=nodes),
            Model(name="NN_TABU", time_limit=300, model_type=4, nodes_count=nodes),
        ]

        for run_idx in range(N_RUNS):
            seed = random.randint(0, 10**9)
            for model in models:
                tasks.append((model, seed, run_idx))

    print(f"Starting {len(tasks)} runs in parallel...")

    with concurrent.futures.ProcessPoolExecutor() as executor:
        futures = [executor.submit(run_model, *task) for task in tasks]
        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                print(result)
            except Exception as e:
                print(f"Task generated an exception: {e}")


if __name__ == "__main__":
    main()
