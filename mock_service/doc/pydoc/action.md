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
def execute(action_context: ActionContext, animators: List[Animator])
```

Execute the action with the given parameters.

**Arguments**:

- `action_context` _ActionContext_ - The context in which the action is invoked.
- `animators` _List[Animator]_ - A refernce to the list of all animators in the system. Can be used to add/remove animators.

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

