"""Model-derived timing illustration, not an event trace or a mean decomposition."""
from __future__ import annotations
import argparse
import json
from pathlib import Path

from generate_b11_delay_figure import model_airtime
import matplotlib.pyplot as plt
from matplotlib.patches import Patch, Rectangle
from matplotlib.ticker import FuncFormatter


def draw(output: Path):
    direct = model_airtime(51, 12)
    access = model_airtime(51, 9)
    backhaul = model_airtime(70, 9)
    dedup = .1
    rows = [
        {"title": "(a) Bezpośrednio: ED–GW", "before": "Bez procedury dostępu\ndo retransmitera", "y": 2,
         "segments": [(direct, "tx", "ED → GW: SF12, 51 B\n2,793 s"), (dedup, "dedup", "")]},
        {"title": "(b) EP: ED–RL–GW", "before": "Wybudzenie RL\ni oczekiwanie związane\nz dryfem zegara", "y": 1,
         "segments": [(access, "tx", "ED → RL\n0,390 s"), (access, "relay", "RL → GW\n0,390 s"), (dedup, "dedup", "")]},
        {"title": "(c) TS/TS0: ED–RL–GW", "before": "WOR, WOR ACK\ni odstępy czasowe\nprzed zgłoszeniem UL", "y": 0,
         "segments": [(access, "tx", "ED → RL\n0,390 s"), (1., "wait", "Zwłoka przekazania RL\n1,000 s"),
                      (backhaul, "relay", "RL → GW\n0,472 s"), (dedup, "dedup", "")]},
    ]
    colors = {"tx": "#447da4", "relay": "#4c9679", "wait": "#e6ad54", "dedup": "#676b74"}
    with plt.rc_context({"font.family": "DejaVu Sans", "font.size": 11, "pdf.fonttype": 42}):
        fig = plt.figure(figsize=(9.6, 5.8))
        pre = fig.add_axes([.018, .225, .255, .60])
        ax = fig.add_axes([.305, .225, .676, .60], sharey=pre)
        for axis in (pre, ax):
            axis.set_ylim(-.57, 2.54)
            axis.set_yticks([])
        pre.set_xlim(0, 1)
        pre.axis("off")
        ax.set_xlim(0, 3.2)
        ax.spines[["top", "right", "left"]].set_visible(False)
        ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}".replace(".", ",")))
        ax.set_xticks([0, .5, 1., 1.5, 2., 2.5, 3.])
        ax.grid(axis="x", alpha=.15, zorder=0)
        ax.set_xlabel("Czas od zgłoszenia ramki UL do radia ED [s]", labelpad=10)
        ax.axvline(0, color="#222222", linestyle=":", linewidth=1.5)
        ax.text(0, 1.06, "POCZĄTEK LICZNIKA", transform=ax.transAxes, fontsize=10, fontweight="bold")
        ax.text(1, 1.06, "Odcinek mierzony — wspólna skala czasu", transform=ax.transAxes,
                fontsize=10, ha="right")
        pre.text(0, 1.06, "PRZED POMIAREM\nbez skali czasu", transform=pre.transAxes,
                 fontsize=10, color="#5b616b", va="bottom")
        endings = {}
        for row in rows:
            y = row["y"]
            pre.text(0, y+.34, row["title"], fontsize=11, fontweight="bold")
            pre.add_patch(Rectangle((0, y-.26), .94, .49, facecolor="#f3f4f5", edgecolor="#c9cdd2", linewidth=.7))
            pre.text(.04, y-.02, row["before"], fontsize=10, color="#535b65", va="center", linespacing=1.25)
            pre.annotate("", xy=(1.09, y), xytext=(.95, y), annotation_clip=False,
                         arrowprops={"arrowstyle": "->", "lw": 1, "color": "#777777"})
            start = 0
            for length, kind, label in row["segments"]:
                ax.barh(y, length, left=start, height=.26, color=colors[kind], edgecolor="white", linewidth=.65, zorder=3)
                if label:
                    ax.text(start+length/2, y-.20, label, ha="center", va="top", fontsize=9.5, linespacing=1.3)
                start += length
            endings[row["title"]] = start
            ax.plot([start, start], [y-.14, y+.27], color="#363b43", lw=.9, zorder=5)
            ax.text(start+.025, y+.20, f"{start:.3f} s".replace(".", ","), va="bottom", fontsize=11, fontweight="bold")
        fig.suptitle("Skąd bierze się różnica czasu doręczenia?", x=.5, y=.986, fontsize=14)
        fig.text(.5, .925, "Zwykły UL: 51 B; odcinki przez RL: SF9; bez kolejek i zakłóceń.", fontsize=10, ha="center")
        handles = [Patch(facecolor=colors[k], label=v) for k,v in (
            ("tx", "Pierwsza transmisja z ED"), ("relay", "Przekazanie z RL"),
            ("wait", "Zwłoka zaplanowana w TS011"), ("dedup", "Deduplikacja LNS: 0,100 s"))]
        fig.legend(handles=handles, loc="lower center", bbox_to_anchor=(.5, .050), ncol=2, frameon=False,
                   fontsize=10, columnspacing=2.5, handlelength=1.5)
        fig.text(.5, .014, "TS i TS0: ta sama sekwencja dla przyjętej ramki. Brak tokenu oznacza odrzucenie, nie oczekiwanie.",
                 ha="center", fontsize=9, color="#4b5563")
        output.mkdir(parents=True, exist_ok=True)
        for extension in ("pdf", "png"):
            fig.savefig(output / f"B11_przebieg_czasowy.{extension}", dpi=240, bbox_inches="tight", pad_inches=.06)
        plt.close(fig)
    details = {"source": "LoRaPacket::calcMsgDuration; LogicRelay::handleAccessUplink; LNS dedupHoldoff",
               "kind": "model-derived idealised single-frame sequence, not simulation trace or empirical mean",
               "pre_measurement_not_to_scale": True, "includes_queueing": False,
               "segments": rows, "total_times_s": endings,
               "TS_minus_EP_s": 1+backhaul-access}
    (output / "B11_przebieg_czasowy.json").write_text(json.dumps(details, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(details["total_times_s"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    draw(parser.parse_args().output)
