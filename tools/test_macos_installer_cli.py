#!/usr/bin/env python3
"""Smoke/integration checks for scripts/install_acrobat_plugin_macos.sh actions."""

from __future__ import annotations

import os
import subprocess
import tempfile
import json
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "install_acrobat_plugin_macos.sh"


def run(cmd: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(REPO_ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )


def write_executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="aimp-installer-cli-") as tmp:
        root = Path(tmp)
        fake_home = root / "home"
        fake_home.mkdir(parents=True, exist_ok=True)

        stubs = root / "stubs"
        stubs.mkdir(parents=True, exist_ok=True)
        xdg_log = root / "xdg-open.log"

        write_executable(stubs / "sudo", "#!/usr/bin/env bash\n\"$@\"\n")
        write_executable(stubs / "xattr", "#!/usr/bin/env bash\nexit 0\n")
        write_executable(stubs / "codesign", "#!/usr/bin/env bash\nexit 0\n")
        write_executable(
            stubs / "xdg-open",
            f"#!/usr/bin/env bash\necho \"$1\" >> {xdg_log}\nexit 0\n",
        )

        base_env = os.environ.copy()
        base_env["HOME"] = str(fake_home)
        base_env["USER"] = "tester"
        base_env["PATH"] = f"{stubs}:{base_env.get('PATH', '')}"

        invalid = run(["bash", str(SCRIPT), "--action", "invalid"], env=base_env)
        if invalid.returncode == 0:
            raise SystemExit(f"Expected invalid --action to fail, got output:\n{invalid.stdout}")

        listed = run(
            ["bash", str(SCRIPT), "--action", "list-targets", "--install-dir", str(root / "listed-target")],
            env=base_env,
        )
        if listed.returncode != 0:
            raise SystemExit(f"list-targets action failed:\n{listed.stdout}")
        if "install_dir=" not in listed.stdout or "uninstall_targets:" not in listed.stdout:
            raise SystemExit("list-targets output did not include expected fields.")
        listed_json = run(
            [
                "bash",
                str(SCRIPT),
                "--action",
                "list-targets",
                "--install-dir",
                str(root / "listed-target"),
                "--json",
            ],
            env=base_env,
        )
        if listed_json.returncode != 0:
            raise SystemExit(f"list-targets --json failed:\n{listed_json.stdout}")
        parsed_listed = json.loads(listed_json.stdout)
        if parsed_listed.get("action") != "list-targets":
            raise SystemExit("list-targets --json returned unexpected action value.")

        status_before = run(
            ["bash", str(SCRIPT), "--action", "status", "--install-dir", str(root / "status-target")],
            env=base_env,
        )
        if status_before.returncode != 0:
            raise SystemExit(f"status action failed:\n{status_before.stdout}")
        if "status=not-installed" not in status_before.stdout:
            raise SystemExit("status action should report not-installed before test installation.")
        status_before_json = run(
            ["bash", str(SCRIPT), "--action", "status", "--install-dir", str(root / "status-target"), "--json"],
            env=base_env,
        )
        if status_before_json.returncode != 0:
            raise SystemExit(f"status --json failed:\n{status_before_json.stdout}")
        parsed_status_before = json.loads(status_before_json.stdout)
        if parsed_status_before.get("status") != "not-installed":
            raise SystemExit("status --json should report not-installed before test installation.")
        status_gate_before_ok = run(
            ["bash", str(SCRIPT), "--action", "status", "--install-dir", str(root / "status-target"), "--require-not-installed"],
            env=base_env,
        )
        if status_gate_before_ok.returncode != 0:
            raise SystemExit(f"status --require-not-installed should pass before install:\n{status_gate_before_ok.stdout}")
        status_gate_before_fail = run(
            ["bash", str(SCRIPT), "--action", "status", "--install-dir", str(root / "status-target"), "--require-installed"],
            env=base_env,
        )
        if status_gate_before_fail.returncode == 0:
            raise SystemExit("status --require-installed should fail before install.")

        # open-folder should create and open the requested directory.
        open_dir = root / "open-folder-target"
        opened = run(
            ["bash", str(SCRIPT), "--action", "open-folder", "--install-dir", str(open_dir)],
            env=base_env,
        )
        if opened.returncode != 0:
            raise SystemExit(f"open-folder action failed:\n{opened.stdout}")
        if not open_dir.exists():
            raise SystemExit("open-folder action did not create target directory.")
        if not xdg_log.exists() or str(open_dir) not in xdg_log.read_text(encoding="utf-8"):
            raise SystemExit("open-folder action did not call xdg-open with target directory.")

        # uninstall should remove files from install-dir and user-level known path.
        install_dir = root / "acrobat-plugins"
        install_dir.mkdir(parents=True, exist_ok=True)
        direct_target = install_dir / "AcrobatImpositionPlugin.api"
        direct_target.write_text("plugin", encoding="utf-8")

        user_target = fake_home / "Library" / "Application Support" / "Adobe" / "Acrobat" / "DC" / "Plug-ins"
        user_target.mkdir(parents=True, exist_ok=True)
        (user_target / "AcrobatImpositionPlugin.api").write_text("plugin", encoding="utf-8")

        removed = run(
            ["bash", str(SCRIPT), "--action", "uninstall", "--install-dir", str(install_dir)],
            env=base_env,
        )
        if removed.returncode != 0:
            raise SystemExit(f"uninstall action failed:\n{removed.stdout}")
        if direct_target.exists() or (user_target / "AcrobatImpositionPlugin.api").exists():
            raise SystemExit("uninstall action did not remove expected plugin files.")

        # install should copy plugin file to install-dir (via sudo stub).
        plugin_src = root / "AcrobatImpositionPlugin.api"
        plugin_src.write_text("binary", encoding="utf-8")
        acrobat_app = root / "Adobe Acrobat.app"
        (acrobat_app / "Contents").mkdir(parents=True, exist_ok=True)
        installed = run(
            [
                "bash",
                str(SCRIPT),
                "--action",
                "install",
                "--acrobat-app",
                str(acrobat_app),
                "--plugin-source",
                str(plugin_src),
            ],
            env=base_env,
        )
        if installed.returncode != 0:
            raise SystemExit(f"install action failed:\n{installed.stdout}")
        installed_target = acrobat_app / "Contents" / "Plug-ins" / "AcrobatImpositionPlugin.api"
        if not installed_target.exists():
            raise SystemExit("install action did not copy plugin target.")

        status_after = run(
            ["bash", str(SCRIPT), "--action", "status", "--acrobat-app", str(acrobat_app)],
            env=base_env,
        )
        if status_after.returncode != 0:
            raise SystemExit(f"status after install failed:\n{status_after.stdout}")
        if "status=installed" not in status_after.stdout:
            raise SystemExit("status action should report installed after install.")
        status_after_json = run(
            ["bash", str(SCRIPT), "--action", "status", "--acrobat-app", str(acrobat_app), "--json"],
            env=base_env,
        )
        if status_after_json.returncode != 0:
            raise SystemExit(f"status after install --json failed:\n{status_after_json.stdout}")
        parsed_status_after = json.loads(status_after_json.stdout)
        if parsed_status_after.get("status") != "installed" or parsed_status_after.get("installed_count", 0) < 1:
            raise SystemExit("status --json should report installed with count >= 1 after install.")
        status_gate_after_ok = run(
            ["bash", str(SCRIPT), "--action", "status", "--acrobat-app", str(acrobat_app), "--require-installed"],
            env=base_env,
        )
        if status_gate_after_ok.returncode != 0:
            raise SystemExit(f"status --require-installed should pass after install:\n{status_gate_after_ok.stdout}")
        status_gate_after_fail = run(
            ["bash", str(SCRIPT), "--action", "status", "--acrobat-app", str(acrobat_app), "--require-not-installed"],
            env=base_env,
        )
        if status_gate_after_fail.returncode == 0:
            raise SystemExit("status --require-not-installed should fail after install.")

        removed_by_app = run(
            [
                "bash",
                str(SCRIPT),
                "--action",
                "uninstall",
                "--acrobat-app",
                str(acrobat_app),
            ],
            env=base_env,
        )
        if removed_by_app.returncode != 0:
            raise SystemExit(f"uninstall via --acrobat-app failed:\n{removed_by_app.stdout}")
        if installed_target.exists():
            raise SystemExit("uninstall via --acrobat-app did not remove plugin target.")

        dry_run_install = run(
            [
                "bash",
                str(SCRIPT),
                "--action",
                "install",
                "--acrobat-app",
                str(acrobat_app),
                "--plugin-source",
                str(plugin_src),
                "--dry-run",
            ],
            env=base_env,
        )
        if dry_run_install.returncode != 0:
            raise SystemExit(f"dry-run install failed:\n{dry_run_install.stdout}")
        if "DRY-RUN" not in dry_run_install.stdout:
            raise SystemExit("dry-run install output did not include DRY-RUN marker.")
        if installed_target.exists():
            raise SystemExit("dry-run install should not create plugin target.")

    print("macOS installer CLI action checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
