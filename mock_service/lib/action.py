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
from typing import Any, Callable, List, NamedTuple, Optional

from lib.animator import Animator, RepeatMode, ValueAnimator
from lib.datapoint import MockedDataPoint
from lib.trigger import TriggerResult
from lib.types import ExecutionContext

log = logging.getLogger("action")


class ActionContext(NamedTuple):
    """Context in which an action is run."""

    trigger: TriggerResult
    execution_context: ExecutionContext
    datapoint: MockedDataPoint


class Action(ABC):
    """A single action of a behavior which can be executed."""

    @abstractmethod
    def execute(
        self,
        action_context: ActionContext,
        animators: List[Animator],
    ):
        """Execute the action with the given parameters.

        Args:
            action_context (ActionContext): The context in which the action is invoked.
            animators (List[Animator]): A refernce to the list of all animators in the system. Can be used to add/remove animators.
        """
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
        self._animator: Optional[Animator] = None
        self._previously_resolved_target_value = None
        self._repeat_mode = repeat_mode
        self._target_value_resolver = target_value_resolver

        self._values_resolved = False

    def _resolve_target_values(self, context: ActionContext):
        """Resolve all dynamic target values.

        Args:
            context (ActionContext): The context in which to resolve the target values.
        """
        for i in range(len(self._values)):
            self._values[i] = self._target_value_resolver(context, self._values[i])

        self._values_resolved = True

    def execute(
        self,
        action_context: ActionContext,
        animators: List[Animator],
    ):
        if not self._values_resolved and self._target_value_resolver is not None:
            self._resolve_target_values(action_context)

        # remove previous reference of this animator instance
        if self._animator is not None and self._animator in animators:
            animators.remove(self._animator)

        self._animator = ValueAnimator(
            self._values,
            self._duration,
            self._repeat_mode,
            lambda x: action_context.datapoint.set_value(x),
        )
        animators.append(self._animator)


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
        self._target_value_resolved = False

    def execute(
        self,
        action_context: ActionContext,
        _: List[Animator],
    ):
        if not self._target_value_resolved and self._target_value_resolver is not None:
            self._value = self._target_value_resolver(action_context, self._value)

        action_context.datapoint.set_value(self._value)
