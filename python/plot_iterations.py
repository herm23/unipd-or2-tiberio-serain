import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# =========================
# CONFIG
# =========================
MAX_POINTS = 50
HALF = MAX_POINTS // 2

colors = {
    "nn": "green",
    "2opt": "red",
    "kick": "pink"
}

ALPHA_LIGHT = 0.03
ALPHA_STRONG = 0.5

# =========================
# LOAD + CLEAN
# =========================
df = pd.read_csv("../cost.csv", names=["a", "b", "c"], skiprows=1)
df["c"] = df["c"].astype(str).str.strip()

# =========================
# COMPRESS SEGMENTS
# =========================
TOL = 1e-9  # tolleranza per "uguaglianza"

new_rows = []
gap_positions = []

labels = df["c"].values
start = 0

for i in range(1, len(df) + 1):
    if i == len(df) or labels[i] != labels[i-1]:
        segment = df.iloc[start:i]
        seg_len = len(segment)

        a_var = segment["a"].max() - segment["a"].min()
        b_var = segment["b"].max() - segment["b"].min()

        is_flat = (a_var < TOL) and (b_var < TOL)

        if seg_len > MAX_POINTS and is_flat:
            head = segment.iloc[:HALF]
            tail = segment.iloc[-HALF:]

            new_rows.append(head)

            gap_pos = sum(len(x) for x in new_rows)
            gap_positions.append((gap_pos, seg_len))

            new_rows.append(pd.DataFrame({
                "a": [np.nan],
                "b": [np.nan],
                "c": [segment["c"].iloc[0]]
            }))

            new_rows.append(tail)
        else:
            # 👉 NON comprimere se i dati cambiano
            new_rows.append(segment)

        start = i

df = pd.concat(new_rows, ignore_index=True)
x = df.index

# =========================
# PLOT
# =========================
plt.figure(figsize=(12, 6))

plt.plot(x, df["a"], label="a", zorder=3)
plt.plot(x, df["b"], label="b", zorder=3)

# =========================
# BACKGROUND SEGMENTS
# =========================
labels = df["c"].values
start = 0

for i in range(1, len(df) + 1):
    if i == len(df) or labels[i] != labels[start]:
        length = i - start
        current_label = labels[start]

        if current_label == "kick":
            alpha = ALPHA_STRONG
        elif current_label == "nn" and length == 1:
            alpha = ALPHA_STRONG
        else:
            alpha = ALPHA_LIGHT

        plt.axvspan(start, i - 1, color=colors[current_label], alpha=alpha, zorder=1)

        start = i

# =========================
# GAP VISUALIZATION
# =========================
ymax = plt.ylim()[1]

for pos, original_len in gap_positions:
    skipped = original_len - MAX_POINTS

    # linea verticale
    plt.axvline(pos, color="black", linestyle="--", alpha=0.5, zorder=2)

    # etichetta
    plt.text(
        pos,
        ymax,
        f"-{skipped}",
        rotation=90,
        va="top",
        ha="center",
        fontsize=8,
        alpha=0.7
    )

# =========================
# FINAL
# =========================
plt.legend()
plt.xlabel("Index (compressed)")
plt.ylabel("Values")
plt.title("Compressed segments with highlighted rare events")

plt.tight_layout()
plt.show()