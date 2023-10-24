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
from abc import ABC, abstractmethod
from typing import Any, Callable, List, Optional, NamedTuple

from lib.animator import Animator, RepeatMode, ValueAnimator
from lib.datapoint import DataPoint
from lib.trigger import TriggerResult
from lib.types import ExecutionContext

log = logging.getLogger("action")


class ActionContext(NamedTuple):
    """Context in which an action is run."""

    trigger: TriggerResult
    execution_context: ExecutionContext
    datapoint: DataPoint


class Action(ABC):
    """A single action of a behavior which can be executed."""

    @abstractmethod
    def execute(
        self,
        action_context: ActionContext,
    ):
        """Execute the action with the given parameters.

        Args:
            action_context (ActionContext): The context in which the action is invoked.
        """
        pass

    @abstractmethod
    def __eq__(self, other) -> bool:
        """Compare if the actions are equal."""
        pass


class AnimationAction(Action):
    """An action which animates a datapoint value."""

    def __init__(
        self,
        duration: float,
        repeat_mode: RepeatMode,
        values: List[Any],
        target_value_resolver: Optional[Callable[[ActionContext, Any], Any]] = None,
    ):
        super().__init__()

        self._duration = duration
        self._values: List[Any] = values
        self._resolved_values = self._values.copy()
        self._animator: Optional[Animator] = None
        self._previously_resolved_target_value = None
        self._repeat_mode = repeat_mode
        self._target_value_resolver = target_value_resolver

    def _resolve_target_values(self, context: ActionContext):
        """Resolve all dynamic target values.

        Args:
            context (ActionContext): The context in which to resolve the target values.
        """
        for i in range(len(self._values)):
            self._resolved_values[i] = self._target_value_resolver(
                context, self._values[i]
            )

    def execute(
        self,
        action_context: ActionContext,
    ):
        if not action_context.datapoint.has_discrete_value_type():
            if self._target_value_resolver is not None:
                self._resolve_target_values(action_context)

            self._animator = ValueAnimator(
                self._resolved_values,
                self._duration,
                self._repeat_mode,
                lambda x: action_context.datapoint.set_value(x),
            )
        else:
            log.error("Datapoint for animation has discrete value")

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, AnimationAction)
            and self._duration == other._duration
            and self._values == other._values
            and self._repeat_mode == other._repeat_mode
            and self._target_value_resolver == other._target_value_resolver
        )


class SetAction(Action):
    """An action which sets the value of a datapoint."""

    def __init__(
        self,
        value: Any,
        target_value_resolver: Optional[Callable[[ActionContext, Any], Any]] = None,
    ):
        super().__init__()

        self._value = value
        self._target_value_resolver = target_value_resolver

    def execute(
        self,
        action_context: ActionContext,
    ):
        self._resolved_value = self._value
        if self._target_value_resolver is not None:
            self._resolved_value = self._target_value_resolver(
                action_context, self._value
            )
        if self._resolved_value is not None:
            action_context.datapoint.set_value(self._resolved_value)

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, SetAction)
            and self._value == other._value
            and self._target_value_resolver == other._target_value_resolver
        )
