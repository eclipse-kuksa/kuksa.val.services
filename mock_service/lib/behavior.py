# /********************************************************************************
# * Copyright (c) 2023 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/

import logging
from typing import Any, Callable

from lib.action import Action
from lib.trigger import Trigger, TriggerResult
from lib.types import ExecutionContext
from lib.action import ActionContext

SERVICE_NAME = "mock_service"

log = logging.getLogger(SERVICE_NAME)


class Behavior:
    """Programmable behavior of a mocked datapoint."""

    def __init__(
        self,
        trigger: Trigger,
        condition: Callable[[ExecutionContext], bool],
        action: Action,
    ):
        self._trigger = trigger
        self._condition = condition
        self._action = action

    def check_trigger(self, execution_context: ExecutionContext) -> TriggerResult:
        """Check the activation of the behavior's trigger."""
        return self._trigger.check(execution_context)

    def is_condition_fulfilled(self, execution_context: ExecutionContext) -> bool:
        """Check the condition of the behavior."""
        return self._condition(execution_context)

    def get_trigger_type(self) -> Any:
        """Return the type of the trigger."""
        return type(self._trigger)

    def execute(
        self,
        action_context: ActionContext,
    ):
        """Execute the programmed action."""
        self._action.execute(action_context)

    def __eq__(self, other):
        if isinstance(other, Behavior):
            return (
                self._trigger == other._trigger
                and self._condition == other._condition
                and self._action == other._action
            )
        return False

    def __ne__(self, other):
        return not self.__eq__(other)
