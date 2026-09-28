"""Mean ED current plot: EP and legacy TS, from validated existing runs."""
from pathlib import Path
import argparse
import json
import numpy as np
from generate_b11_delay_figure import fit
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

KEYS = ("ep4312388", "ts011")
NAMES = ("EP4312388", "TS011 (legacy)")
COLORS = ("#1f77b4", "#e8750a")
LOADS = np.arange(55,551,55)


def select_fit(y):
    candidates = []
    grid = np.linspace(55,550,500)
    for family in ("linear","quadratic","exponential"):
        prediction,params = fit(LOADS,y,family)
        errors = []
        for j in range(10):
            keep = np.arange(10) != j
            trained,_ = fit(LOADS[keep],y[keep],family)
            errors.append(float(trained(LOADS[j]))-y[j])
        values = prediction(grid)
        candidates.append(dict(family=family,parameters=params,
            loo_rmse_uA=float(np.sqrt(np.mean(np.square(errors)))),
            admissible=bool(np.all(values>=0) and np.all(np.diff(values)<=1e-8))))
    selected = min((v for v in candidates if v["admissible"]),key=lambda v:v["loo_rmse_uA"])
    prediction,_ = fit(LOADS,y,selected["family"])
    return prediction,dict(selected=selected,candidates=candidates)


def main(source,output):
    report = json.loads(source.read_text(encoding="utf-8"))
    fits = {}
    output.mkdir(parents=True,exist_ok=True)
    with plt.rc_context({"font.family":"DejaVu Sans","font.size":10,"pdf.fonttype":42,
                         "axes.spines.top":False,"axes.spines.right":False}):
        fig,ax = plt.subplots(figsize=(7.3,4.7))
        fig.subplots_adjust(left=.12,right=.985,bottom=.14,top=.97)
        for key,name,color,marker in zip(KEYS,NAMES,COLORS,("o","s")):
            rows = [report["summary"][key][str(n)]["total_uA"] for n in LOADS]
            means = np.array([v["mean"] for v in rows])
            predict,fits[key] = select_fit(means)
            grid = np.linspace(55,550,500)
            ax.errorbar(LOADS,means,yerr=[v["ci95"][1]-v["mean"] for v in rows],linestyle="none",
                color=color,marker=marker,markersize=4.8,capsize=2.7,linewidth=1,label=name,zorder=5)
            ax.plot(grid,predict(grid),"--",color=color,linewidth=1.25,alpha=.85)
        ax.axhline(25.3,color="#8d969f",linestyle=":",linewidth=1)
        ax.text(555,25.39,"prąd stanu bezczynności: 25,3 µA",ha="right",va="bottom",fontsize=8,color="#626b73")
        for n in (55,385):
            ax.axvline(n,color="#8d969f",linestyle=":",linewidth=.8,zorder=0)
        ax.set(xlim=(35,570),ylim=(25,29.4),xlabel=r"liczba urządzeń $N_{\mathrm{ED}}$",
            ylabel=r"średni prąd jednego ED [$\mu$A]")
        ax.set_xticks([55,165,275,385,495,550])
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v,_:f"{v:g}".replace(".",",")))
        ax.grid(alpha=.17)
        ax.legend(loc="upper right",frameon=False,fontsize=9.5)
        for extension in ("pdf","png"):
            fig.savefig(output/f"B11_prad_ED.{extension}",dpi=240,bbox_inches="tight",pad_inches=.06)
        plt.close(fig)
    source.with_name("B11_ed_current_fits.json").write_text(json.dumps(fits,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(fits,ensure_ascii=False,indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--source",type=Path,default=Path(r"C:\Omnet\simulation_results\review_chapter1\B11_ed_energy_analysis.json"))
    p.add_argument("--output",type=Path,default=Path(__file__).parent/"energy_figures")
    args = p.parse_args()
    main(args.source,args.output)
