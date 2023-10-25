<a id="lib.dsl"></a>

# lib.dsl

<a id="lib.dsl.mock_datapoint"></a>

#### mock\_datapoint

```python
def mock_datapoint(path: str,
                   initial_value: Any,
                   behaviors: List[Behavior] = None)
```

Mock a single datapoint.

**Arguments**:

- `path` _str_ - The VSS path of the datapoint to mock.
- `initial_value` _Any_ - The initial value the datapoint will assume on registration.
- `behaviors` _List[Behavior]_ - A list of programmed behaviors to execute for the mocked datapoint.

<a id="lib.dsl.create_behavior"></a>

#### create\_behavior

```python
def create_behavior(
    trigger: Trigger,
    action: Action,
    condition: Callable[[ExecutionContext],
                        bool] = lambda _: True) -> Behavior
```

Create a behavior from the given parameters. It is mandatory to call this when
using Python DSL in order to derive required datapoints.

**Arguments**:

- `trigger` _Trigger_ - A trigger which will invoke this behavior. Can either be `ClockTrigger` or `EventTrigger`.
- `action` _Action, optional_ - An action to execute once the trigger activates
  and the condition evaluates to true. Defaults to None.
- `condition` __type_, optional_ - A condition which needs to be fulfilled **AFTER**
  the trigger has activated in order to execute the action. Defaults to lambda_:True.
  

**Returns**:

- `Behavior` - A new behavior with the given trigger, condition and action.

<a id="lib.dsl.add_behavior"></a>

#### add\_behavior

```python
def add_behavior(behavior: Behavior, path: str)
```

Add a given behavior to an already mocked datapoint

**Arguments**:

- `behavior` _Behavior_ - The behavior that shall be added
- `path` _str_ - The already mocked datapoint to whom shall be added

<a id="lib.dsl.get_datapoint_value"></a>

#### get\_datapoint\_value

```python
def get_datapoint_value(context: ExecutionContext,
                        path: str,
                        default: Any = 0) -> Any
```

Get the value of a datapoint or, if its not available yet, a default value is returned.

**Arguments**:

- `context` _ExecutionContext_ - The execution context from which the datapoint can be retrieved.
- `path` _str_ - The path of the VSS datapoint.
- `default` _Any, optional_ - Optional default value if there is no value for the datapoint. Defaults to 0.
  

**Returns**:

- `Any` - The value of the datapoint at the specified path or the provided default value.

<a id="lib.dsl.create_set_action"></a>

#### create\_set\_action

```python
def create_set_action(value: Any) -> SetAction
```

Create a SetAction with dynamic value resolution. See `__resolve_value`
for documentation of value resolution.

**Arguments**:

- `value` _Any_ - The value to set or a dynamic literal.
  

**Returns**:

- `SetAction` - The created SetAction.

<a id="lib.dsl.create_animation_action"></a>

#### create\_animation\_action

```python
def create_animation_action(
        values: List[Any],
        duration: float,
        repeat_mode: RepeatMode = RepeatMode.ONCE) -> AnimationAction
```

Create an AnimationAction with dynamic value resolution.
Values are dynamically resolved at trigger activation time of the owning behavior.
See `__resolve_value` for documentation of value resolution.

**Arguments**:

- `values` _List[Any]_ - The list of values to animate over. May contain dynamic values.
- `duration` _float_ - The total duration of the animation in seconds.
- `repeat_mode` _RepeatMode, optional_ - The repeat mode of the animation. Defaults to RepeatMode.ONCE.
  

**Returns**:

- `AnimationAction` - The created AnimationAction.

<a id="lib.dsl.create_event_trigger"></a>

#### create\_event\_trigger

```python
def create_event_trigger(type: EventType,
                         path: Optional[str] = None) -> EventTrigger
```

Create an EventTrigger for the mocked datapoint in context of this call OR the explicitly passed one.

**Arguments**:

- `type` _EventType_ - The type of event which will activate the trigger.
- `path` _Optional[str]_ - The data point which shall raise the event.
  If not set defaults to the mocked data point in context of the call.
  

**Returns**:

- `EvenTrigger` - The created EventTrigger.

<a id="lib.dsl.delete_behavior_of_mocked_datapoint"></a>

#### delete\_behavior\_of\_mocked\_datapoint

```python
def delete_behavior_of_mocked_datapoint(behavior: Behavior, path: str)
```

Delete one behavior for a mocked datapoint

**Arguments**:

- `behavior` _Behavior_ - The behavior which shall be removed.
- `path` _str_ - The data point which behavior shall be removed.

<a id="lib.dsl.delete_mocked_datapoint"></a>

#### delete\_mocked\_datapoint

```python
def delete_mocked_datapoint(path: str)
```

Delete all behaviors for a mocked datapoint

**Arguments**:

- `path` _str_ - The path for which all behaviors shall be removed.

<a id="lib.dsl.delete_all_mocked_datapoints"></a>

#### delete\_all\_mocked\_datapoints

```python
def delete_all_mocked_datapoints()
```

Delete all mocked datapoints from the mock

