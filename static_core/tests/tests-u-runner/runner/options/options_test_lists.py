from dataclasses import dataclass
from functools import cached_property
from typing import Dict, Optional

from runner.enum_types.configuration_kind import ArchitectureKind, SanitizerKind
from runner.options.decorator_value import value, _to_bool, _to_enum, _to_str
from runner.options.options_groups import GroupsOptions


@dataclass
class TestListsOptions:
    __DEFAULT_ARCH = ArchitectureKind.AMD64
    __DEFAULT_SAN = SanitizerKind.NONE
    __DEFAULT_FILTER = "*"

    def __str__(self) -> str:
        return _to_str(self, TestListsOptions, 1)

    def to_dict(self) -> Dict[str, object]:
        return {
            "architecture": self.architecture.value.upper(),
            "sanitizer": self.sanitizer.value.upper(),
            "explicit-file": self.explicit_file,
            "explicit-list": self.explicit_list,
            "filter": self.filter,
            "skip-test-lists": self.skip_test_lists,
            "update-excluded": self.update_excluded,
            "update-expected": self.update_expected,
            "groups": self.groups.to_dict(),
        }

    groups = GroupsOptions()

    @cached_property
    @value(
        yaml_path="test-lists.architecture",
        cli_name="test_list_arch",
        cast_to_type=lambda x: _to_enum(x, ArchitectureKind)
    )
    def architecture(self) -> ArchitectureKind:
        return TestListsOptions.__DEFAULT_ARCH

    @cached_property
    @value(
        yaml_path="test-lists.sanitizer",
        cli_name="test_list_san",
        cast_to_type=lambda x: _to_enum(x, SanitizerKind)
    )
    def sanitizer(self) -> SanitizerKind:
        return TestListsOptions.__DEFAULT_SAN

    @cached_property
    @value(yaml_path="test-lists.explicit-file", cli_name="test_file")
    def explicit_file(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="test-lists.explicit-list", cli_name="test_list")
    def explicit_list(self) -> Optional[str]:
        return None

    @cached_property
    @value(yaml_path="test-lists.filter", cli_name="filter")
    def filter(self) -> str:
        return TestListsOptions.__DEFAULT_FILTER

    @cached_property
    @value(yaml_path="test-lists.skip-test-lists", cli_name="skip_test_lists", cast_to_type=_to_bool)
    def skip_test_lists(self) -> bool:
        return False

    @cached_property
    @value(yaml_path="test-lists.update-excluded", cli_name="update_excluded", cast_to_type=_to_bool)
    def update_excluded(self) -> bool:
        return False

    @cached_property
    @value(yaml_path="test-lists.update-expected", cli_name="update_expected", cast_to_type=_to_bool)
    def update_expected(self) -> bool:
        return False

    def get_command_line(self) -> str:
        options = [
            f'--test-list-arch={self.architecture.value}'
            if self.architecture != TestListsOptions.__DEFAULT_ARCH else '',
            f'--test-list-san={self.sanitizer.value}'
            if self.sanitizer != TestListsOptions.__DEFAULT_SAN else '',
            f'--test-file="{self.explicit_file}"' if self.explicit_file is not None else '',
            f'--test-list="{self.explicit_list}"' if self.explicit_list is not None else '',
            f'--filter="{self.filter}"' if self.filter != TestListsOptions.__DEFAULT_FILTER else '',
            '--skip-test-lists' if self.skip_test_lists else '',
            '--update-excluded' if self.update_excluded else '',
            '--update-expected' if self.update_expected else ''
        ]
        return ' '.join(options)
