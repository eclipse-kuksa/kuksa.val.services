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
from typing import Any, Callable, List

import numpy
import logging
from scipy import interpolate

log = logging.getLogger("animator")


class RepeatMode(Enum):
    """Enumeration of available repeat modes."""

    ONCE = (0,)
    REPEAT = 1


class Animator(ABC):
    """Abstract base class for implementing animators."""

    @abstractmethod
    def tick(self, delta_time: float):
        """Advances the time for the animator by `delta_time`."""
        pass

    @abstractmethod
    def is_done(self) -> bool:
        """Return True if the animator is done playing the animation."""
        pass

    @abstractmethod
    def __eq__(self, other) -> bool:
        """Return True if the animator is equal."""
        pass


class ValueAnimator(Animator):
    """Animates between equally distanced values over time."""

    def __init__(
        self,
        values: List,
        duration: float,
        repeat_mode: RepeatMode,
        value_update_callback: Callable[[Any], None] = None,
    ):
        super().__init__()
        self._values = values
        x = numpy.linspace(0, duration, len(values))
        self._interpolation = interpolate.interp1d(
            x,
            values,
            bounds_error=False,
            fill_value=(self._values[0], self._values[-1]),
        )
        self._duration = duration
        self._anim_time = 0.0
        self._done = False
        self._repeat_mode = repeat_mode
        self._value_update_callback = value_update_callback
        self._value = self._values[0]

    def tick(self, delta_time: float):
        if self._done:
            return

        self._anim_time = self._anim_time + delta_time

        if self._anim_time > self._duration:
            if self._repeat_mode == RepeatMode.ONCE:
                self._anim_time = self._duration
                self._done = True
            elif self._repeat_mode == RepeatMode.REPEAT:
                self._anim_time = self._anim_time - self._duration

        self._value = self._interpolation(self._anim_time)

        if self._value_update_callback is not None:
            self._value_update_callback(self._value)

    def is_done(self) -> bool:
        return self._done

    def get_value(self):
        return self._value

    def __eq__(self, other) -> bool:
        return (
            isinstance(other, ValueAnimator)
            and self._duration == other._duration
            and self._anim_time == other._anim_time
            and self._done == other._done
            and self._repeat_mode == other._repeat_mode
            and self._value == other._value
            and self._value_update_callback == other._value_update_callback
            and self._interpolation == other._interpolation
        )
