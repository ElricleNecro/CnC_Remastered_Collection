#!/usr/bin/env python3

from dataclasses import asdict, dataclass
from json import dump
from pathlib import Path
from typing import Callable, Iterable, Iterator


CPP_STANDARD: str = "c++14"
CODE_REPO: list[Path] = [
    # Path("CnCTDRAMapEditor"),
    Path("RedAlert"),
    Path("TiberianDawn"),
]
WDIR: Path = Path("build").absolute()

@dataclass
class File:
    """Configuration for a specific file.

    Configuration for a file to be written in the compile_commands.json.

    Attributes:
        directory: working directory for the compilation command.
        file: main translation unit for this step (file name to be compiled).
        arguments: argv-style list of string.
        command: shell escaped command line to compile the unit (prefer using arguments).
        output: output of the compilation unit.
    """
    directory: Path | str
    file: Path | str
    arguments: list[str]
    command: str | None = None
    output: str | None = None

    def asdict(self) -> dict[str, str]:
        d = asdict(self)

        if d["command"] is None:
            del d["command"]

        if d["output"] is None:
            del d["output"]

        if isinstance(d["directory"], Path):
            d["directory"] = str(d["directory"])

        if isinstance(d["file"], Path):
            d["file"] = str(d["file"])

        return d


def flat_map[T, U](f: Callable[[T], Iterator[U]], xs: Iterator[T]) -> Iterator[U]:
    return (y for ys in xs for y in f(ys))


def peek[T](f: Callable[[T], None], xs: Iterator[T]) -> Iterator[T]:
    for x in xs:
        f(x)
        yield x


def recursive(p: Path) -> Iterator[Path]:
    if p.is_file():
        yield p
    else:
        for inner in flat_map(recursive, p.iterdir()):
            yield inner


def format_args(f: Path, repo: Path):
    return [
        "/usr/bin/clang++",
        f"-std={CPP_STANDARD}",
        f"-I{f.absolute().parent.relative_to(WDIR, walk_up=True)}",
        f"-I{repo.absolute().relative_to(WDIR, walk_up=True)}",
        "-c",
        f"{f.absolute().relative_to(WDIR, walk_up=True)}",
        "-o",
        f"{f.with_suffix(".o").name}",
    ]


if __name__ == "__main__":
    unit_list: list[File] = []

    WDIR.mkdir(parents=True, exist_ok=True)

    for repo in CODE_REPO:
        files = flat_map(recursive, repo.iterdir())
        files = filter(lambda f: f.suffix not in {".rc", ".RC", ".vcxproj", ".filters"}, files)
        files = map(lambda f: f.rename(f.parent / f.name.lower()), files)
        files = filter(lambda f: f.suffix in {".CPP", ".cpp"}, files)
        files = map(lambda f: File(directory=WDIR, file=f.absolute().relative_to(WDIR, walk_up=True), arguments=format_args(f, repo)), files)
        files = map(File.asdict, files)
        unit_list.extend(files)

    with open("compile_commands.json", "w") as f:
        dump(unit_list, f, indent=4)
