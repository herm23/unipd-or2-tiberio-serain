import os
import re
import csv

# Root directory containing instance folders
ROOT_DIR = "path/to/your/root"  # <-- change this

# Regex to extract Best Cost
COST_PATTERN = re.compile(r"Best Cost \(zbest\): ([\d\.]+)")

# Data structure: {instance: {system: cost}}
data = {}

# Collect all system names (filenames)
all_systems = set()

# Iterate over instance folders
for instance in os.listdir(ROOT_DIR):
    instance_path = os.path.join(ROOT_DIR, instance)

    if not os.path.isdir(instance_path):
        continue

    data[instance] = {}

    # Iterate over files (systems)
    for filename in os.listdir(instance_path):
        file_path = os.path.join(instance_path, filename)

        if not os.path.isfile(file_path):
            continue

        all_systems.add(filename)

        # Read file and extract Best Cost
        with open(file_path, "r") as f:
            content = f.read()

        match = COST_PATTERN.search(content)
        if match:
            cost = float(match.group(1))
        else:
            cost = None  # or use 0 / "" if preferred

        data[instance][filename] = cost

# Sort systems for consistent column order
all_systems = sorted(all_systems)

# Write CSV
output_file = "results.csv"

with open(output_file, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)

    # Header
    header = ["Instance"] + all_systems
    writer.writerow(header)

    # Rows
    for instance in sorted(data.keys()):
        row = [instance]
        for system in all_systems:
            row.append(data[instance].get(system, ""))
        writer.writerow(row)

print(f"CSV saved to {output_file}")
