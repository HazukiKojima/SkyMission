"""Generate benchmark layouts from the production binding metadata, not hand-copied fields."""
import pathlib
import re
import sys

repo, output = map(pathlib.Path, sys.argv[1:])
entries = []
for kind in ("AircraftAI", "AircraftFlight", "Ocean", "Sky"):
    source = (repo / f"External/REDoxWrapper/Generated/Generated{kind}Parameters.cs").read_text(encoding="utf-8-sig")
    names = re.findall(r'"([^"]+)"', re.search(r"FieldNames\s*=\s*\{(.*?)\}", source, re.S)[1])
    lengths = list(map(int, re.findall(r"\d+", re.search(r"FieldLengths\s*=\s*\{(.*?)\}", source, re.S)[1])))
    assert len(names) == len(lengths)
    entries.append((kind, list(zip(names, lengths))))
source = (repo / "External/REDoxWrapper/REDoxApi.cs").read_text(encoding="utf-8-sig")
body = source.split("bool valid =", 1)[1].split("if (!valid", 1)[0]
parents = {"root": "", "colorGrading": "/colorGrading", "blackLift": "/colorGrading/blackLift", "highlight": "/colorGrading/highlight"}
retro = []
for match in re.finditer(r'Append(Number|Array)\((\w+), "([^"]+)"(?:, (\d+))?', body):
    operation, parent, name, length = match.groups()
    retro.append((parents.get(parent, "/" + parent) + "/" + name, int(length) if operation == "Array" else 1))
assert sum(length for _, length in retro) == 50
entries.append(("RetroFilm", retro))
text = "const std::unordered_map<std::string, Layout>& GetLayouts() {\nstatic const std::unordered_map<std::string, Layout> layouts = {\n"
for kind, fields in entries:
    text += '    {"' + kind + '", {' + ",".join('{"' + name + '",' + str(length) + "}" for name, length in fields) + "}},\n"
text += "};\nreturn layouts;\n}\n"
(output / "Layouts.inc").write_text(text, encoding="utf-8")
