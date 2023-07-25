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
from typing import Any, Callable, Dict, List, Optional

from lib.action import Action, ActionContext, AnimationAction, SetAction
from lib.animator import RepeatMode
from lib.behavior import Behavior, ExecutionContext
from lib.trigger import EventTriggerResult, Trigger, EventType, EventTrigger


_mocked_datapoints: List[Dict] = list()
_required_datapoint_paths: List[str] = list()

log = logging.getLogger("dsl")


def mock_datapoint(path: str, initial_value: Any, behaviors: List[Behavior] = list()):
    """Mock a single datapoint.

    Args:
        path (str): The VSS path of the datapoint to mock.
        initial_value (Any): The initial value the datapoint will assume on registration.
        behaviors (List[Behavior]): A list of programmed behaviors to execute for the mocked datapoint.
    """
    _mocked_datapoints.append(
        {"path": path, "initial_value": initial_value, "behaviors": behaviors}
    )


def create_behavior(
    trigger: Trigger,
    action: Action,
    condition: Callable[[ExecutionContext], bool] = lambda _: True,
) -> Behavior:
    """Create a behavior from the given parameters. It is mandatory to call this when
    using Python DSL in order to derive required datapoints.

    Args:
        trigger (Trigger): A trigger which will invoke this behavior. Can either be `ClockTrigger` or `EventTrigger`.
        action (Action, optional): An action to execute once the trigger activates
            and the condition evaluates to true. Defaults to None.
        condition (_type_, optional): A condition which needs to be fulfilled **AFTER**
            the trigger has activated in order to execute the action. Defaults to lambda_:True.

    Returns:
        Behavior: A new behavior with the given trigger, condition and action.
    """
    return Behavior(trigger, condition, action)


def get_datapoint_value(context: ExecutionContext, path: str, default: Any = 0) -> Any:
    """Get the value of a datapoint or, if its not available yet, a default value is returned.

    Args:
        context (ExecutionContext): The execution context from which the datapoint can be retrieved.
        path (str): The path of the VSS datapoint.
        default (Any, optional): Optional default value if there is no value for the datapoint. Defaults to 0.

    Returns:
        Any: The value of the datapoint at the specified path or the provided default value.
    """
    if path not in _mocked_datapoints:
        _required_datapoint_paths.append(path)
    curr_vals = context.client.get_current_values([path, ])
    if curr_vals != None:
        return curr_vals[path].value
    
    return default


def __resolve_value(action_context: ActionContext, value: Any) -> Any:
    """If the value provided is a dynamic value - that is a string which is either:
    * $self => resolves to the mocked datapoint's value
    * $event.(value|path) => resolves to the triggering event's value or path
    * $<DatapointPath> => resolves to the value at the specified datapoint path
    The value is replaced with the value of the desired value at this moment in time.

    Args:
        action_context (ActionContext): The context from which to retrieve dynamic values.
        value (Any): The value to resolve.

    Raises:
        ValueError: If a dynamic string literal is found that is not supported by the resolver.

    Returns:
        Any: The value itself or the resolved value.
    """

    if isinstance(value, str) and value.startswith("$"):
        if value == "$self":
            curr_vals = action_context.execution_context.client.get_current_values([action_context.datapoint.path,])
            if curr_vals != None:
                return curr_vals[action_context.datapoint.path].value
            else:
                return 0 
        elif value == "$event.value":
            if isinstance(action_context.trigger, EventTriggerResult):
                return action_context.trigger.get_event().value
            else:
                raise ValueError(
                    f"Unsupported literal: {value!r} in non event-triggered behavior!"
                )
        elif value.startswith("$"):
            curr_vals = action_context.execution_context.client.get_current_values([value[1:],])
            if curr_vals != None:
                return curr_vals[value[1:]].value
            else:
                return 0 
    return value


def create_set_action(value: Any) -> SetAction:
    """Create a SetAction with dynamic value resolution. See `__resolve_value`
    for documentation of value resolution.

    Args:
        value (Any): The value to set or a dynamic literal.

    Returns:
        SetAction: The created SetAction.
    """
    return SetAction(value, __resolve_value)


def create_animation_action(
    values: List[Any], duration: float, repeat_mode: RepeatMode = RepeatMode.ONCE
) -> AnimationAction:
    """Create an AnimationAction with dynamic value resolution.
    Values are dynamically resolved at trigger activation time of the owning behavior.
    See `__resolve_value` for documentation of value resolution.

    Args:
        values (List[Any]): The list of values to animate over. May contain dynamic values.
        duration (float): The total duration of the animation in seconds.
        repeat_mode (RepeatMode, optional): The repeat mode of the animation. Defaults to RepeatMode.ONCE.

    Returns:
        AnimationAction: The created AnimationAction.
    """
    return AnimationAction(duration, repeat_mode, values, __resolve_value)

def create_EventTrigger(type: EventType, path: Optional[str] = None) -> EventTrigger:
    """Create a SetAction with dynamic value resolution. See `__resolve_value`
    for documentation of value resolution.

    Args:
        value (Any): The value to set or a dynamic literal.

    Returns:
        SetAction: The created SetAction.
    """
    if path is not None:
        _required_datapoint_paths.append(path)
    return EventTrigger(type, path)
