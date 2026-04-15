import os
import re
import csv

ROOT_DIR = r"C:\Users\filip\CLionProjects\unipd-or2-tiberio-serain\python\runs"

COST_PATTERN = re.compile(r"Best Cost \(zbest\): ([\d\.]+)")

models = ["NN_2OPT_MS", "NN_VNS", "NN_TABU"]

# =========================
# PARSING + CSV PER NODES
# =========================

for nodes in os.listdir(ROOT_DIR):
    nodes_path = os.path.join(ROOT_DIR, nodes)

    if not os.path.isdir(nodes_path):
        continue

    print(f"Processing nodes = {nodes}")

    # {seed: {model: cost}}
    data = {}

    for model in models:
        model_path = os.path.join(nodes_path, model)

        if not os.path.isdir(model_path):
            continue

        for seed_folder in os.listdir(model_path):
            seed_path = os.path.join(model_path, seed_folder)

            if not os.path.isdir(seed_path):
                continue

            # estrai seed
            try:
                seed = int(seed_folder.split("_")[1])
            except:
                continue

            if seed not in data:
                data[seed] = {}

            # file unico dentro
            for filename in os.listdir(seed_path):
                file_path = os.path.join(seed_path, filename)

                if not os.path.isfile(file_path):
                    continue

                with open(file_path, "r") as f:
                    content = f.read()

                match = COST_PATTERN.search(content)
                cost = float(match.group(1)) if match else None

                data[seed][model] = cost

    # =========================
    # WRITE CSV (righe = run)
    # =========================

    output_file = f"results_{nodes}.csv"

    with open(output_file, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)

        # header
        writer.writerow(models)

        # righe: una per seed
        for seed in sorted(data.keys()):
            row = [data[seed].get(m, "") for m in models]
            writer.writerow(row)

    print(f"Saved {output_file}")