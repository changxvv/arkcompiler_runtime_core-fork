from dataclasses import dataclass
from functools import cached_property
from os import path
from typing import Dict, Optional

from runner.enum_types.qemu import QemuKind
from runner.enum_types.verbose_format import VerboseKind, VerboseFilter
from runner.options.decorator_value import value, _to_qemu, _to_bool, _to_enum, _to_str, _to_path, _to_int, \
    _to_processes
from runner.options.options_coverage import CoverageOptions
from runner.reports.report_format import ReportFormat


# pylint: disable=too-many-public-methods
@dataclass
class GeneralOptions:
    __DEFAULT_PROCESSES = 1
    __DEFAULT_CHUNKSIZE = 32
    __DEFAULT_GC_TYPE = "g1-gc"
    __DEFAULT_GC_BOMBING_FREQUENCY = 0
    __DEFAULT_HEAP_VERIFIER = "fail_on_verification"
    __DEFAULT_REPORT_FORMAT = ReportFormat.LOG
    __DEFAULT_VERBOSE = VerboseKind.NONE
    __DEFAULT_VERBOSE_FILTER = VerboseFilter.NEW_FAILURES
    __DEFAULT_QEMU = QemuKind.NONE

    def __str__(self) -> str:
        return _to_str(self, GeneralOptions, 1)

    def to_dict(self) -> Dict[str, object]:
        return {
            "build": self.build,
            "processes": self.processes,
            "test-root": self.test_root,
            "list-root": self.list_root,
            "work-dir": self.work_dir,
            "ets-stdlib-root": self.ets_stdlib_root,
            "show-progress": self.show_progress,
            "gc_type": self.gc_type,
            "full-gc-bombing-frequency": self.full_gc_bombing_frequency,
            "run_gc_in_place": self.run_gc_in_place,
            "heap-verifier": self.heap_verifier,
            "report-format": self.report_format.value.upper(),
            "verbose": self.verbose.value.upper(),
            "verbose-filter": self.verbose_filter.value.upper(),
            "coverage": self.coverage.to_dict(),
            "force-download": self.force_download,
            "bco": self.bco,
            "qemu": self.qemu.value.upper(),
            "with-js": self.with_js,
        }

    @cached_property
    @value(
        yaml_path="general.generate-config",
        cli_name="generate_config",
        cast_to_type=_to_path
    )
    def generate_config(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="general.processes", cli_name="processes", cast_to_type=_to_processes)
    def processes(self) -> int:
        return GeneralOptions.__DEFAULT_PROCESSES

    @cached_property
    def chunksize(self) -> int:
        return GeneralOptions.__DEFAULT_CHUNKSIZE

    @cached_property
    @value(yaml_path="general.build", cli_name="build_dir", cast_to_type=_to_path, required=True)
    def build(self) -> Optional[str]:
        return None

    @cached_property
    def panda_source_root(self) -> str:
        # This file is expected to be located at path:
        # $PANDA_SOURCE/tests/tests-u-runner/runner/options/options_general.py
        path_parts = __file__.split(path.sep)[:-5]
        return path.sep.join(path_parts)

    @cached_property
    @value(yaml_path="general.test-root", cli_name="test_root", cast_to_type=_to_path)
    def test_root(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="general.list-root", cli_name="list_root", cast_to_type=_to_path)
    def list_root(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="general.work-dir", cli_name="work_dir", cast_to_type=_to_path)
    def work_dir(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="general.ets-stdlib-root", cli_name="ets_stdlib_root", cast_to_type=_to_path)
    def ets_stdlib_root(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="general.show-progress", cli_name="progress", cast_to_type=_to_bool)
    def show_progress(self) -> bool:
        return False

    @cached_property
    @value(yaml_path="general.gc_type", cli_name="gc_type")
    def gc_type(self) -> str:
        return GeneralOptions.__DEFAULT_GC_TYPE

    @cached_property
    @value(yaml_path="general.full-gc-bombing-frequency", cli_name="full_gc_bombing_frequency", cast_to_type=_to_int)
    def full_gc_bombing_frequency(self) -> int:
        return GeneralOptions.__DEFAULT_GC_BOMBING_FREQUENCY

    @cached_property
    @value(yaml_path="general.run_gc_in_place", cli_name="run_gc_in_place", cast_to_type=_to_bool)
    def run_gc_in_place(self) -> bool:
        return False

    @cached_property
    @value(yaml_path="general.heap-verifier", cli_name="heap_verifier")
    def heap_verifier(self) -> str:
        return GeneralOptions.__DEFAULT_HEAP_VERIFIER

    @cached_property
    @value(
        yaml_path="general.report-format",
        cli_name="report_formats",
        cast_to_type=lambda x: _to_enum(x, ReportFormat)
    )
    def report_format(self) -> ReportFormat:
        return GeneralOptions.__DEFAULT_REPORT_FORMAT

    @cached_property
    @value(
        yaml_path="general.verbose",
        cli_name="verbose",
        cast_to_type=lambda x: _to_enum(x, VerboseKind)
    )
    def verbose(self) -> VerboseKind:
        return GeneralOptions.__DEFAULT_VERBOSE

    @cached_property
    @value(
        yaml_path="general.verbose-filter",
        cli_name="verbose_filter",
        cast_to_type=lambda x: _to_enum(x, VerboseFilter)
    )
    def verbose_filter(self) -> VerboseFilter:
        return GeneralOptions.__DEFAULT_VERBOSE_FILTER

    coverage = CoverageOptions()

    @cached_property
    @value(yaml_path="general.force-download", cli_name="force_download", cast_to_type=_to_bool)
    def force_download(self) -> bool:
        return False

    @cached_property
    @value(yaml_path="general.bco", cli_name="bco", cast_to_type=_to_bool)
    def bco(self) -> bool:
        return True

    @cached_property
    @value(yaml_path="general.with_js", cli_name="with_js", cast_to_type=_to_bool)
    def with_js(self) -> bool:
        return True

    @cached_property
    @value(yaml_path="general.qemu", cli_name=["arm64_qemu", "arm32_qemu"], cast_to_type=_to_qemu)
    def qemu(self) -> QemuKind:
        return GeneralOptions.__DEFAULT_QEMU

    def get_command_line(self) -> str:
        if self.qemu == QemuKind.ARM64:
            _qemu = '--arm64-qemu'
        elif self.qemu == QemuKind.ARM32:
            _qemu = '--arm32-qemu'
        else:
            _qemu = ''
        options = [
            f'--generate-config="{self.generate_config}"' if self.generate_config else '',
            f'--processes={self.processes}' if self.processes != GeneralOptions.__DEFAULT_PROCESSES else '',
            f'--chunksize={self.chunksize}' if self.chunksize != GeneralOptions.__DEFAULT_CHUNKSIZE else '',
            f'--build-dir="{self.build}"',
            f'--test-root="{self.test_root}"' if self.test_root else '',
            f'--list-root="{self.list_root}"' if self.list_root else '',
            f'--work-dir="{self.work_dir}"' if self.work_dir else '',
            f'--ets-stdlib-root="{self.ets_stdlib_root}"' if self.ets_stdlib_root else '',
            '--show-progress' if self.show_progress else '',
            f'--gc-type="{self.gc_type}"' if self.gc_type != GeneralOptions.__DEFAULT_GC_TYPE else '',
            f'--full-gc-bombing-frequency={self.full_gc_bombing_frequency}'
            if self.full_gc_bombing_frequency != GeneralOptions.__DEFAULT_GC_BOMBING_FREQUENCY else '',
            '--no-run-gc-in-place' if self.run_gc_in_place else '',
            f'--heap-verifier="{self.heap_verifier}"'
            if self.heap_verifier != GeneralOptions.__DEFAULT_HEAP_VERIFIER else '',
            f'--report-format={self.report_format.value}'
            if self.report_format != GeneralOptions.__DEFAULT_REPORT_FORMAT else '',
            f'--verbose={self.verbose.value}'
            if self.verbose != GeneralOptions.__DEFAULT_VERBOSE else '',
            f'--verbose-filter={self.verbose_filter.value}'
            if self.verbose_filter != GeneralOptions.__DEFAULT_VERBOSE_FILTER else '',
            self.coverage.get_command_line(),
            '--force-download' if self.force_download else '',
            '--no-bco' if not self.bco else '',
            '--no-js' if not self.with_js else '',
            _qemu
        ]
        return ' '.join(options)
