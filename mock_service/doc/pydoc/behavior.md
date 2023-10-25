<a id="lib.behavior"></a>

# lib.behavior

<a id="lib.behavior.Behavior"></a>

## Behavior Objects

```python
class Behavior()
```

Programmable behavior of a mocked datapoint.

<a id="lib.behavior.Behavior.check_trigger"></a>

#### check\_trigger

```python
def check_trigger(execution_context: ExecutionContext) -> TriggerResult
```

Check the activation of the behavior's trigger.

<a id="lib.behavior.Behavior.is_condition_fulfilled"></a>

#### is\_condition\_fulfilled

```python
def is_condition_fulfilled(execution_context: ExecutionContext) -> bool
```

Check the condition of the behavior.

<a id="lib.behavior.Behavior.get_trigger_type"></a>

#### get\_trigger\_type

```python
def get_trigger_type() -> Any
```

Return the type of the trigger.

<a id="lib.behavior.Behavior.execute"></a>

#### execute

```python
def execute(action_context: ActionContext)
```

Execute the programmed action.

