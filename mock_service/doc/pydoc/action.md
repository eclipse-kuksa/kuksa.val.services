<a id="lib.action"></a>

# lib.action

<a id="lib.action.ActionContext"></a>

## ActionContext Objects

```python
class ActionContext(NamedTuple)
```

Context in which an action is run.

<a id="lib.action.Action"></a>

## Action Objects

```python
class Action(ABC)
```

A single action of a behavior which can be executed.

<a id="lib.action.Action.execute"></a>

#### execute

```python
@abstractmethod
def execute(action_context: ActionContext)
```

Execute the action with the given parameters.

**Arguments**:

- `action_context` _ActionContext_ - The context in which the action is invoked.

<a id="lib.action.Action.__eq__"></a>

#### \_\_eq\_\_

```python
@abstractmethod
def __eq__(other) -> bool
```

Compare if the actions are equal.

<a id="lib.action.AnimationAction"></a>

## AnimationAction Objects

```python
class AnimationAction(Action)
```

An action which animates a datapoint value.

<a id="lib.action.SetAction"></a>

## SetAction Objects

```python
class SetAction(Action)
```

An action which sets the value of a datapoint.

