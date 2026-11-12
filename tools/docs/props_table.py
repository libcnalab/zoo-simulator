#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rewrites the "Props" table of docs/CONSTRUCTION.md from data/construction/props.json."""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load(name):
    data = json.loads((ROOT / "data/construction" / f"{name}.json").read_text())
    return data[name] if isinstance(data, dict) else data


def num(value):
    return "%g" % value


def main():
    foods = {f["id"]: f["name"] for f in load("foods")}
    rows = []
    for p in load("props"):
        kind = p["role"]
        role = kind.replace("_", " ").capitalize()
        if p["biome"] and kind in ("tree", "bush"):
            role += ", " + p["biome"]
        keys = []
        if kind in ("tree", "bush"):
            keys.append("Cover %s m²; shade %s m²" % (num(p["vegetationM2"]), num(p["shadeM2"])))
            if kind == "tree":
                keys.append("%s m tall; trunk %s m" % (num(p["heightM"]), num(p["radiusM"])))
        elif kind == "feeder":
            text = "/".join(foods.get(f, f) for f in p["foods"]) + ", %s kg" % num(p["capacity"])
            if p["feedHeightM"] > 1.5:
                text += " at %s m" % num(p["feedHeightM"])
            keys.append(text)
        elif kind == "water_trough":
            keys.append("%s L" % num(p["capacity"]))
        elif kind == "bench":
            keys.append("%s seats" % num(p["capacity"]))
        elif kind == "bin":
            keys.append("%s items" % num(p["capacity"]))
        elif kind == "lamp":
            keys.append("%s m" % num(p["heightM"]))
        elif kind == "info_board":
            keys.append("Public education")
        elif kind == "decoration":
            keys.append("Beauty %.2f" % p["beauty"])
        if kind not in ("tree", "bush") and p["enrichment"] > 0:
            if p["vegetationM2"] > 0:
                keys.append("cover %s m²" % num(p["vegetationM2"]))
            keys.append("Enrichment %.2f" % p["enrichment"])
        if not p["blocksWalking"]:
            keys.append("walk-through")
        rows.append("| %s | %s | %s | %s | $%s |"
                    % (p["id"], p["name"], role, "; ".join(keys), format(int(p["cost"]), ",")))
    path = ROOT / "docs/CONSTRUCTION.md"
    lines = path.read_text().split("\n")
    start = lines.index("## Props")
    end = next(n for n in range(start + 1, len(lines)) if lines[n].startswith("## "))
    head = [
        "## Props",
        "",
        "Trees and bushes add plant cover and shade to the enclosure they stand in, and a little",
        "enrichment (0.03 per tree, 0.02 per bush). A tree occupies only its trunk on the ground, so",
        "animals and visitors walk under its crown. This table is generated: run",
        "`tools/docs/props_table.py` after editing `data/construction/props.json`.",
        "",
        "| ID | Name | Role | Key values | Cost |",
        "|---|---|---|---|---:|",
    ]
    lines[start:end] = head + rows + [""]
    path.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
