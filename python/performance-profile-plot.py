import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Input CSV
CSV_FILE = r"./runs/results_1000.csv"

# Load data
df = pd.read_csv(CSV_FILE)

# Convert to numeric (in case of empty cells)
df = df.apply(pd.to_numeric, errors="coerce")

# Drop runs where all models failed
df = df.dropna(how="all")

# Number of problems (runs)
n_problems = df.shape[0]

# Compute best value per run
best = df.min(axis=1)

# Compute performance ratios
ratios = df.divide(best, axis=0)

# Replace NaN (missing values) with +inf
ratios = ratios.fillna(np.inf)

# Tau values (x-axis)
tau_max = np.nanmax(ratios.replace(np.inf, np.nan).values)
tau_values = np.linspace(1, tau_max * 1.1, 200)

# Plot
plt.figure(figsize=(8, 6))

for system in ratios.columns:
    rho = []
    for tau in tau_values:
        count = np.sum(ratios[system] <= tau)
        rho.append(count / n_problems)

    plt.step(tau_values, rho, where="post", label=system)

# Styling
plt.xlabel(r"$\tau$")
plt.ylabel(r"Performance Profile $\rho(\tau)$")
plt.title("Performance Profile")
plt.legend()
plt.grid(True)

plt.tight_layout()

# SOLO SHOW (no save)
plt.show()