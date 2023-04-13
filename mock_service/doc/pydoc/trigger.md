<a id="lib.trigger"></a>

# lib.trigger

<a id="lib.trigger.TriggerResult"></a>

## TriggerResult Objects

```python
class TriggerResult()
```

Result of a trigger check.

<a id="lib.trigger.TriggerResult.is_active"></a>

#### is\_active

```python
def is_active() -> bool
```

Return True if the trigger is active.

<a id="lib.trigger.EventTriggerResult"></a>

## EventTriggerResult Objects

```python
class EventTriggerResult(TriggerResult)
```

Result of an event trigger activation.

<a id="lib.trigger.ClockTriggerResult"></a>

## ClockTriggerResult Objects

```python
class ClockTriggerResult(TriggerResult)
```

Result of a clock trigger activation.

<a id="lib.trigger.Trigger"></a>

## Trigger Objects

```python
class Trigger(ABC)
```

Abstract base class of a trigger which leads to activation of behaviors.

<a id="lib.trigger.Trigger.check"></a>

#### check

```python
@abstractmethod
def check(execution_context: ExecutionContext) -> TriggerResult
```

Check if the trigger is activated.

<a id="lib.trigger.Trigger.is_recurring"></a>

#### is\_recurring

```python
@abstractmethod
def is_recurring() -> bool
```

Return if the trigger is recurring. If True it activates more than once.

<a id="lib.trigger.ClockTrigger"></a>

## ClockTrigger Objects

```python
class ClockTrigger(Trigger)
```

A clock-based trigger.

<a id="lib.trigger.ClockTrigger.__init__"></a>

#### \_\_init\_\_

```python
def __init__(interval_sec: float, is_recurring: bool = False)
```

Create a clock-based trigger which activates after `interval_sec`.
If `is_recurring` is set to True, the trigger will activate every
`interval_sec`

<a id="lib.trigger.ClockTrigger.reset"></a>

#### reset

```python
def reset()
```

Reset the clock to make the trigger activate again.

<a id="lib.trigger.EventType"></a>

## EventType Objects

```python
class EventType(Enum)
```

All possible event types.

<a id="lib.trigger.EventTrigger"></a>

## EventTrigger Objects

```python
class EventTrigger(Trigger)
```

Data broker event-based trigger.

