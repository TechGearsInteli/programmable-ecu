#!/usr/bin/env python3
"""Compila e testa no computador, sem placa: nucleo da ECU + simulador da HMI.

    python tools/host_build.py            configura, compila e roda os testes
    python tools/host_build.py --sim      ...e depois abre o simulador (janela)
    python tools/host_build.py --capturas docs/docs/img/hmi
                                          ...e gera as capturas de tela em PNG
    python tools/host_build.py --docker   faz tudo num container Linux (Docker), sem
                                          compilador no Windows (sem a janela interativa)

Precisa de CMake, Ninja e um compilador C. Se nao houver compilador no PATH,
usa o Zig (um compilador C completo distribuido pelo PyPI, sem instalador):

    python -m pip install --user cmake ninja ziglang
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "host"


def find_tool(name: str, pip_module: str) -> str:
    path = shutil.which(name)
    if path:
        return path
    try:
        mod = __import__(pip_module)
        bin_dir = getattr(mod, "BIN_DIR", None) or getattr(mod, "CMAKE_BIN_DIR", None)
        if bin_dir:
            exe = Path(bin_dir) / (name + (".exe" if os.name == "nt" else ""))
            if exe.exists():
                return str(exe)
    except ImportError:
        pass
    sys.exit(f"{name} nao encontrado. Instale com: python -m pip install --user {pip_module}")


def compiler_env() -> dict:
    env = dict(os.environ)
    if env.get("CC"):
        return env
    for cc in ("gcc", "clang", "cl"):
        if shutil.which(cc):
            return env
    try:
        import ziglang  # noqa: F401  (pacote do PyPI com o compilador Zig)
        zig = Path(ziglang.__file__).parent / ("zig.exe" if os.name == "nt" else "zig")
        env["CC"] = f"{zig} cc"
        print(f"[host_build] usando o Zig como compilador C: {zig}")
        return env
    except ImportError:
        sys.exit("Nenhum compilador C encontrado. Instale um ou rode: python -m pip install --user ziglang")


def run(cmd, env):
    print("[host_build] " + " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], check=True, env=env, cwd=ROOT)


DOCKER_IMAGE = "alpine:3.20"


def docker_build(args):
    """Mesmo build dentro de um container Linux. Util quando o Windows nao deixa
    rodar executaveis recem-compilados (ex.: Smart App Control ligado)."""
    vols = ["-v", f"{ROOT.as_posix()}:/src"]
    defs = ""
    lvgl_dir = args.lvgl or os.environ.get("ECU_LVGL_DIR")
    if lvgl_dir:
        vols += ["-v", f"{Path(lvgl_dir).resolve().as_posix()}:/deps/lvgl:ro"]
        defs += " -DFETCHCONTENT_SOURCE_DIR_LVGL=/deps/lvgl"
    unity_dir = os.environ.get("ECU_UNITY_DIR")
    if unity_dir:
        vols += ["-v", f"{Path(unity_dir).resolve().as_posix()}:/deps/unity:ro"]
        defs += " -DFETCHCONTENT_SOURCE_DIR_UNITY=/deps/unity"
    script = ("apk add --no-cache build-base cmake ninja git > /dev/null"
              f" && cmake -S /src -B /src/build/linux -G Ninja{defs}"
              " && cmake --build /src/build/linux")
    if not args.sem_testes:
        script += " && ctest --test-dir /src/build/linux --output-on-failure"
    if args.capturas:
        script += " && /src/build/linux/hmi_sim --headless --cenario todos --saida /src/build/linux/capturas > /dev/null"
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    run(["docker", "run", "--rm", *vols, DOCKER_IMAGE, "sh", "-c", script], env)
    if args.capturas:
        run([sys.executable, ROOT / "tools" / "bmp_to_png.py", ROOT / "build" / "linux" / "capturas",
             Path(args.capturas).resolve()], env)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sim", action="store_true", help="abre o simulador depois de compilar")
    ap.add_argument("--capturas", metavar="PASTA", help="gera as capturas de tela (PNG) nesta pasta")
    ap.add_argument("--sem-testes", action="store_true", help="so compila")
    ap.add_argument("--lvgl", metavar="PASTA", help="usa um LVGL 9.2.2 ja baixado (evita baixar de novo)")
    ap.add_argument("--docker", action="store_true", help="compila e testa num container Linux")
    args = ap.parse_args()
    if args.docker:
        docker_build(args)
        return

    cmake = find_tool("cmake", "cmake")
    ninja = find_tool("ninja", "ninja")
    env = compiler_env()

    configure = [cmake, "-S", ROOT, "-B", BUILD, "-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}",
                 "-DCMAKE_BUILD_TYPE=Debug"]
    lvgl_dir = args.lvgl or os.environ.get("ECU_LVGL_DIR")
    if lvgl_dir:
        configure.append(f"-DFETCHCONTENT_SOURCE_DIR_LVGL={Path(lvgl_dir).resolve().as_posix()}")
    unity_dir = os.environ.get("ECU_UNITY_DIR")
    if unity_dir:
        configure.append(f"-DFETCHCONTENT_SOURCE_DIR_UNITY={Path(unity_dir).resolve().as_posix()}")
    run(configure, env)
    run([cmake, "--build", BUILD, "--parallel"], env)

    if not args.sem_testes:
        ctest = str(Path(cmake).with_name("ctest" + (".exe" if os.name == "nt" else "")))
        # --repeat: no Windows o antivirus as vezes segura um .exe recem-criado por um instante
        run([ctest, "--test-dir", BUILD, "--output-on-failure", "--repeat", "until-pass:3"], env)

    sim = BUILD / ("hmi_sim.exe" if os.name == "nt" else "hmi_sim")
    if args.capturas:
        out = Path(args.capturas).resolve()
        tmp = BUILD / "capturas"
        run([sim, "--headless", "--cenario", "todos", "--saida", tmp], env)
        run([sys.executable, ROOT / "tools" / "bmp_to_png.py", tmp, out], env)
    if args.sim:
        subprocess.Popen([str(sim)], cwd=BUILD)


if __name__ == "__main__":
    main()
