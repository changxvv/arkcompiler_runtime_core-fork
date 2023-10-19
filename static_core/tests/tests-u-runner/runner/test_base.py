import logging
from datetime import datetime
from enum import Enum
from typing import Dict, Optional, List

from runner.enum_types.params import TestEnv, TestReport
from runner.enum_types.verbose_format import VerboseKind, VerboseFilter
from runner.logger import Log
from runner.reports.report import ReportGenerator
from runner.reports.report_format import ReportFormat

_LOGGER = logging.getLogger("runner.test_base")


class TestStatus(Enum):
    PASSED = "PASSED"
    PASSED_IGNORED = "PASSED IGNORED"
    FAILURE_IGNORED = "FAILURE IGNORED"
    EXCLUDED = "EXCLUDED"
    NEW_FAILURE = "NEW FAILURE"


class Test:
    """
    Test knows how to run test and defines whether the test is failed or passed
    """

    def __init__(self, test_env: TestEnv, test_path: str, flags: List[str], test_id: str):
        self.test_env = test_env
        # full path to the test file
        self.path = test_path
        self.flags = flags
        self.test_id = test_id
        self.update_expected = self.test_env.config.test_lists.update_expected
        self.expected = ""
        # Contains fields output, error, and return_code of the last executed step
        self.report: Optional[TestReport] = None
        # Test result: True if all steps passed, False is any step fails
        self.passed: Optional[bool] = None
        # If the test is mentioned in any ignored_list
        self.ignored = False
        # Test can detect itself as excluded additionally to excluded_tests
        # In such case the test will be counted as `excluded by other reasons`
        self.excluded = False
        # Collect all executable commands
        self.reproduce = ""
        # Time to execute in seconds
        self.time: Optional[float] = None
        # Reports if generated. Key is ReportFormat.XXX. Value is a path to the generated report
        self.reports: Dict[ReportFormat, str] = {}

    def log_cmd(self, cmd):
        self.reproduce += "\n" + ' '.join(cmd)

    def run(self):
        start = datetime.now()
        Log.all(_LOGGER, f"Start to execute: {self.test_id}")

        result = self.do_run()

        finish = datetime.now()
        self.time = (finish - start).total_seconds()

        if result.passed is None:
            return result

        if self.is_output_status():
            Log.default(_LOGGER, f"Finished {self.test_id} - {round(self.time, 2)} sec - {self.status().value}")
        if self.is_output_log():
            self.reproduce += f"\nTo reproduce with URunner run:\n{self.get_command_line()}"
            Log.default(_LOGGER, f"{self.test_id}: steps: {self.reproduce}")
            if self.report:
                Log.default(_LOGGER, f"{self.test_id}: expected output: {self.expected}")
                Log.default(_LOGGER, f"{self.test_id}: actual output: {self.report.output}")
                Log.default(_LOGGER, f"{self.test_id}: actual error: {self.report.error}")
                Log.default(_LOGGER, f"{self.test_id}: actual return code: {self.report.return_code}")
            else:
                Log.default(_LOGGER, f"{self.test_id}: no information about test running neither output nor error")

        report_generator = ReportGenerator(self.test_id, self.test_env)
        self.reports = report_generator.generate_fail_reports(result)

        return result

    def do_run(self):
        return self

    def status(self) -> TestStatus:
        if self.excluded:
            return TestStatus.EXCLUDED
        if self.passed:
            return TestStatus.PASSED_IGNORED if self.ignored else TestStatus.PASSED
        return TestStatus.FAILURE_IGNORED if self.ignored else TestStatus.NEW_FAILURE

    def get_command_line(self) -> str:
        config_cmd = self.test_env.config.get_command_line()
        reproduce_message = [
            f'{self.test_env.config.general.panda_source_root}/tests/tests-u-runner/runner.sh',
            self.test_env.config.general.panda_source_root,
            config_cmd
        ]
        if config_cmd.find('--test-file') < 0:
            reproduce_message.append(f" --test-file {self.test_id}")
        return ' '.join(reproduce_message)

    def is_output_log(self) -> bool:
        """
        Returns True if logs should be logged
        verbose in [ALL, SHORT] and filter == ALL -> True
        verbose in [ALL, SHORT] and filter == KNOWN and status = New or Known or Passed Ignored -> True
        verbose in [ALL, SHORT] and filter == NEW and status = New -> True

        verbose == NONE and status == New -> True
        """
        verbose_filter = self.test_env.config.general.verbose_filter
        verbose = self.test_env.config.general.verbose
        status = self.status()
        if verbose in [VerboseKind.ALL, VerboseKind.SHORT]:
            is_all = verbose_filter == VerboseFilter.ALL
            is_ignored = verbose_filter == VerboseFilter.IGNORED and \
                         status in [TestStatus.NEW_FAILURE, TestStatus.FAILURE_IGNORED, TestStatus.PASSED_IGNORED]
            is_new = verbose_filter == VerboseFilter.NEW_FAILURES and \
                     status == TestStatus.NEW_FAILURE
            return is_all or is_ignored or is_new

        return verbose == VerboseKind.NONE and status == TestStatus.NEW_FAILURE

    def is_output_status(self) -> bool:
        """
        Returns True if status should be logged
        verbose in [ALL, SHORT] -> True

        verbose == NONE and filter == ALL -> True
        verbose == NONE and filter == KNOWN and status = New or Known or Passed Ignored -> True
        verbose == NONE and filter == NEW and status = New -> True
        """
        verbose = self.test_env.config.general.verbose
        verbose_filter = self.test_env.config.general.verbose_filter
        status = self.status()
        if verbose == VerboseKind.NONE:
            is_all = verbose_filter == VerboseFilter.ALL
            is_ignored = verbose_filter == VerboseFilter.IGNORED and \
                         status in [TestStatus.NEW_FAILURE, TestStatus.FAILURE_IGNORED, TestStatus.PASSED_IGNORED]
            is_new = verbose_filter == VerboseFilter.NEW_FAILURES and \
                     status == TestStatus.NEW_FAILURE
            return is_all or is_ignored or is_new

        return True
