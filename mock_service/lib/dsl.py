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
from lib.trigger import EventTrigger, EventTriggerResult, EventType, Trigger

_mocked_datapoints: List[Dict] = list()
_required_datapoint_paths: List[str] = list()

log = logging.getLogger("dsl")


def mock_datapoint(path: str, initial_value: Any, behaviors: List[Behavior] = None):
    """Mock a single datapoint.

    Args:
        path (str): The VSS path of the datapoint to mock.
        initial_value (Any): The initial value the datapoint will assume on registration.
        behaviors (List[Behavior]): A list of programmed behaviors to execute for the mocked datapoint.
    """
    if behaviors is None:
        behaviors = []

    path_exists = any("path" in d and d["path"] == path for d in _mocked_datapoints)
    if not path_exists:
        _mocked_datapoints.append(
            {"path": path, "initial_value": initial_value, "behaviors": behaviors}
        )
    else:
        log.error("Datapoint already mocked please add behavior instead")


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


def add_behavior(
    behavior: Behavior,
    path: str,
):
    """Add a given behavior to an already mocked datapoint

    Args:
        behavior (Behavior): The behavior that shall be added
        path (str): The already mocked datapoint to whom shall be added
    """
    exist = False
    for dict in _mocked_datapoints:
        if dict["path"] == path:
            dict["behaviors"].append(behavior)
            exist = True
            break

    if not exist:
        log.error("Not mocked please add the new datapoint")


def get_datapoint_value(context: ExecutionContext, path: str, default: Any = 0) -> Any:
    """Get the value of a datapoint or, if its not available yet, a default value is returned.

    Args:
        context (ExecutionContext): The execution context from which the datapoint can be retrieved.
        path (str): The path of the VSS datapoint.
        default (Any, optional): Optional default value if there is no value for the datapoint. Defaults to 0.

    Returns:
        Any: The value of the datapoint at the specified path or the provided default value.
    """
    if path not in _required_datapoint_paths:
        _required_datapoint_paths.append(path)
    curr_vals = context.client.get_current_values(
        [
            path,
        ]
    )
    if curr_vals[path] is not None:
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
            return get_datapoint_value(
                action_context.execution_context, action_context.datapoint.path
            )

        elif value == "$event.value":
            if isinstance(action_context.trigger, EventTriggerResult):
                return action_context.trigger.get_event().value
            else:
                raise ValueError(
                    f"Unsupported literal: {value!r} in non event-triggered behavior!"
                )
        elif value.startswith("$"):
            return get_datapoint_value(action_context.execution_context, value[1:])
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


def create_event_trigger(type: EventType, path: Optional[str] = None) -> EventTrigger:
    """Create an EventTrigger for the mocked datapoint in context of this call OR the explicitly passed one.

    Args:
        type (EventType): The type of event which will activate the trigger.
        path (Optional[str]): The data point which shall raise the event.
            If not set defaults to the mocked data point in context of the call.

    Returns:
        EvenTrigger: The created EventTrigger.
    """
    if path is not None:
        if path not in _required_datapoint_paths:
            _required_datapoint_paths.append(path)
    return EventTrigger(type, path)


def delete_behavior_of_mocked_datapoint(behavior: Behavior, path: str):
    """Delete one behavior for a mocked datapoint

    Args:
        behavior (Behavior): The behavior which shall be removed.
        path (str): The data point which behavior shall be removed.
    """
    for dict in _mocked_datapoints:
        if path == dict['path']:
            for saved in dict['behaviors']:
                if saved == behavior:
                    dict['behaviors'].remove(behavior)


def delete_mocked_datapoint(path: str):
    """Delete all behaviors for a mocked datapoint

    Args:
        path (str): The path for which all behaviors shall be removed.
    """
    for dict in _mocked_datapoints:
        if path == dict['path']:
            _mocked_datapoints.remove(dict)


def delete_all_mocked_datapoints():
    """Delete all mocked datapoints from the mock
    """
    _mocked_datapoints.clear()
