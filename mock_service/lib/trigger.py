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

from abc import ABC, abstractmethod
from enum import Enum
from typing import Optional

from lib.types import Event, ExecutionContext


class TriggerResult:
    """Result of a trigger check."""

    def __init__(self, active: bool):
        super().__init__()
        self._is_active = active

    def is_active(self) -> bool:
        """Return True if the trigger is active."""
        return self._is_active


class EventTriggerResult(TriggerResult):
    """Result of an event trigger activation."""

    def __init__(self, active: bool, event: Optional[Event]):
        super().__init__(active)
        self._event = event

    def get_event(self) -> Event:
        return self._event


class ClockTriggerResult(TriggerResult):
    """Result of a clock trigger activation."""

    def __init__(self, active: bool):
        super().__init__(active)


class Trigger(ABC):
    """Abstract base class of a trigger which leads to activation of behaviors."""

    def __init__(self):
        super().__init__()

    @abstractmethod
    def check(self, execution_context: ExecutionContext) -> TriggerResult:
        """Check if the trigger is activated."""
        pass

    @abstractmethod
    def is_recurring(self) -> bool:
        """Return if the trigger is recurring. If True it activates more than once."""
        pass

    @abstractmethod
    def __eq__(self, other) -> bool:
        """Compare if the triggers are equal."""
        pass


class ClockTrigger(Trigger):
    """A clock-based trigger."""

    def __init__(self, interval_sec: float, is_recurring: bool = False):
        """Create a clock-based trigger which activates after `interval_sec`.
        If `is_recurring` is set to True, the trigger will activate every
        `interval_sec`"""
        super().__init__()

        self._interval_sec = interval_sec
        self._is_recurring = is_recurring
        self.reset()

    def is_recurring(self) -> bool:
        return self._is_recurring

    def check(self, execution_context: ExecutionContext) -> TriggerResult:
        if self._expired:
            return ClockTriggerResult(False)

        self._time_left = self._time_left - execution_context.delta_time
        if self._time_left <= 0:
            if self._is_recurring:
                self._time_left = self._interval_sec - self._time_left
            else:
                self._time_left = 0
                self._expired = True

            return ClockTriggerResult(True)
        return ClockTriggerResult(False)

    def reset(self):
        """Reset the clock to make the trigger activate again."""
        self._time_left = self._interval_sec
        self._expired = False

    def __eq__(self, other) -> bool:
        """Compare if the triggers are equal."""
        return (
            isinstance(other, ClockTrigger)
            and self._is_recurring == other._is_recurring
            and self._interval_sec == self._interval_sec
        )


class EventType(Enum):
    """All possible event types."""

    ACTUATOR_TARGET = "actuator_target"
    VALUE = "value"


class EventTrigger(Trigger):
    """Data broker event-based trigger."""

    def __init__(self, event_type: EventType, datapoint_path: Optional[str] = None):
        """Creates a new trigger for the given event type.

        Args:
            event_type (EventType): The type of event which will invoke the trigger.
            datapoint_path (Optional[str], optional): The path of the datapoint which invokes the trigger.
                If not specified, the path of the parent mocked datapoint will be used instead.
                Defaults to None.
        """

        super().__init__()
        self._event_type = event_type
        self._datapoint_path = datapoint_path

    def is_recurring(self) -> bool:
        return True

    def check(self, execution_context: ExecutionContext) -> TriggerResult:
        event_to_remove = None

        if self._datapoint_path is None:
            self._datapoint_path = execution_context.calling_signal_path

        for event in execution_context.pending_event_list:
            if (
                self._event_type.value == event.name
                and event.path == self._datapoint_path
            ):
                event_to_remove = event

        if event_to_remove is not None:
            execution_context.pending_event_list.remove(event_to_remove)

        return EventTriggerResult(event_to_remove is not None, event_to_remove)

    def __eq__(self, other) -> bool:
        """Compare if the triggers are equal."""
        return (
            isinstance(other, EventTrigger)
            and self._datapoint_path == other._datapoint_path
            and self._event_type == self._event_type
        )
