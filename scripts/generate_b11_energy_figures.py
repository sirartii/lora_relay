"""Scientific figures from validated relay charge balances (no new runs)."""
from pathlib import Path
import argparse
import json
import os
os.environ.setdefault("MPLCONFIGDIR", str(Path(os.environ.get("TEMP", "."))/"lora_review_matplotlib"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import numpy as np

KEYS = ("ep4312388", "ts011")
NAMES = ("EP4312388", "TS011 (legacy)")
SERIES_COLORS = ("#1f77b4", "#e8750a")
COMPONENTS = ("Rx", "Tx", "WOR", "Idle", "BaseOffset")
COMP_COLORS = ("#447da4", "#df9953", "#55a082", "#b4beca", "#806c9c")
COMP_NAMES = ("RX: odbiór i nasłuch", "TX: nadawanie", "WOR: gotowość", "Bezczynność", "Pobór bazowy")
LOADS = np.arange(55, 551, 55)


def pl(v, decimals=1):
    return f"{v:.{decimals}f}".replace(".", ",")


def choose_fit(means):
    x = LOADS / 550.
    grid = np.linspace(.1, 1, 500)
    candidates = []
    for degree in (1, 2):
        coef = np.polyfit(x, means, degree)
        errors = []
        for j in range(10):
            keep = np.arange(10) != j
            c = np.polyfit(x[keep], means[keep], degree)
            errors.append(np.polyval(c, x[j])-means[j])
        curve = np.polyval(coef, grid)
        candidates.append(dict(degree=degree, coefficients_descending=coef.tolist(), x_scale=550,
            loo_rmse_uA=float(np.sqrt(np.mean(np.square(errors)))),
            admissible=bool(np.all(curve >= 0) and np.all(np.diff(curve) >= 0))))
    selected = min((v for v in candidates if v["admissible"]), key=lambda v:v["loo_rmse_uA"])
    return {"selected": selected, "candidates": candidates}


def save(fig, folder, name):
    for extension in ("pdf", "png"):
        fig.savefig(folder/f"{name}.{extension}", dpi=240, bbox_inches="tight", pad_inches=.06)
    plt.close(fig)


def main(source, output):
    report = json.loads(source.read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    fits = {}
    with plt.rc_context({"font.family":"DejaVu Sans", "font.size":10,
                         "pdf.fonttype":42, "axes.spines.top":False, "axes.spines.right":False}):
        fig, ax = plt.subplots(figsize=(7.3, 4.15))
        fig.subplots_adjust(left=.115, right=.985, bottom=.155, top=.96)
        grid = np.linspace(55, 550, 500)
        for key, name, color, marker in zip(KEYS, NAMES, SERIES_COLORS, ("o","s","^")):
            rows = [report["summary"][key][str(n)]["total_uA"] for n in LOADS]
            means = np.array([v["mean"] for v in rows])
            fits[key] = choose_fit(means)
            ax.errorbar(LOADS, means, yerr=[v["ci95"][1]-v["mean"] for v in rows],
                        linestyle="none", marker=marker, markersize=4.8, color=color,
                        capsize=2.7, linewidth=1, label=name, zorder=5)
            ax.plot(grid, np.polyval(fits[key]["selected"]["coefficients_descending"],grid/550),
                    "--", linewidth=1.25, color=color, alpha=.85)
        ax.set(xlim=(35,570), ylim=(0,770), xlabel=r"liczba urządzeń $N_{\mathrm{ED}}$",
               ylabel=r"średni prąd jednego RL [$\mu$A]")
        ax.set_xticks([55,165,275,385,495,550])
        for n in (55,385):
            ax.axvline(n, color="#8a929b", linewidth=.8, linestyle=":", zorder=0)
        ax.grid(alpha=.17)
        ax.legend(loc="upper left", frameon=False, fontsize=9.5)
        save(fig,output,"B11_prad_relay")

        # Equal-size circles encode composition, NOT the absolute energy consumed.
        fig, axes = plt.subplots(2, 2, figsize=(7.6, 8.2))
        fig.subplots_adjust(left=.02,right=.98,bottom=.125,top=.91,wspace=.10,hspace=.22)
        for col, load in enumerate((55,385)):
            fig.text(.25 if col==0 else .75,.980,f"{load} ED",ha="center",fontsize=11,fontweight="bold")
        for row,key in enumerate(KEYS):
            for col,load in enumerate((55,385)):
                ax = axes[row,col]
                values = [report["summary"][key][str(load)]["components_uA"][c]["mean"] for c in COMPONENTS]
                total = sum(values)
                active = [(i,v) for i,v in enumerate(values) if v > 1e-10]
                wedges, _ = ax.pie([v for i,v in active], colors=[COMP_COLORS[i] for i,v in active],
                    startangle=90, counterclock=False, radius=1.,
                    wedgeprops={"edgecolor":"white","linewidth":.65})
                # Small adjacent sectors receive percentage-only leader labels.
                outside = {
                    ("ep4312388",385,"Idle"): (-.43,1.12),
                    ("ep4312388",385,"BaseOffset"): (.12,1.12),
                    ("ts011",55,"Tx"): (1.10,.73),
                    ("ts011",55,"BaseOffset"): (-.22,1.12),
                    ("ts011",385,"BaseOffset"): (-.18,1.12),
                }
                for wedge,(i,value) in zip(wedges,active):
                    pct = 100*value/total
                    angle = np.deg2rad((wedge.theta1+wedge.theta2)/2)
                    label = f"{pl(pct)}%"
                    if pct >= 4:
                        radius = .63 if pct >= 15 else .72
                        ax.text(radius*np.cos(angle),radius*np.sin(angle),label,
                            ha="center",va="center",fontsize=8,color="white" if i in (0,2,4) else "#172533")
                    else:
                        ax.annotate(label,xy=(.985*np.cos(angle),.985*np.sin(angle)),
                            xytext=outside[key,load,COMPONENTS[i]],ha="center",va="center",fontsize=7.5,
                            arrowprops={"arrowstyle":"-","color":"#606873","linewidth":.65,
                                        "connectionstyle":"angle3,angleA=0,angleB=90"})
                ax.set_title(f"{NAMES[row]}\n"+r"$\overline{I}_{\mathrm{RL}}$ = "+f"{pl(total)} µA", fontsize=9,pad=9)
                ax.set_xlim(-1.12,1.24)
                ax.set_ylim(-1.04,1.20)
        fig.legend([Patch(facecolor=c) for c in COMP_COLORS],COMP_NAMES,loc="lower center",
            bbox_to_anchor=(.5,.048),ncol=3,frameon=False,fontsize=8,columnspacing=1.4,handlelength=1.3)
        save(fig,output,"B11_energia_skladowe")
    fit_output = source.with_name("B11_energy_fits.json")
    fit_output.write_text(json.dumps(fits,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"figures":str(output),"fits":fits},ensure_ascii=False,indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source",type=Path,default=Path(r"C:\Omnet\simulation_results\review_chapter1\B11_energy_analysis.json"))
    parser.add_argument("--output",type=Path,default=Path(__file__).parent/"energy_figures")
    args = parser.parse_args()
    main(args.source,args.output)
