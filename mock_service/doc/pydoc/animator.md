<a id="lib.animator"></a>

# lib.animator

<a id="lib.animator.RepeatMode"></a>

## RepeatMode Objects

```python
class RepeatMode(Enum)
```

Enumeration of available repeat modes.

<a id="lib.animator.Animator"></a>

## Animator Objects

```python
class Animator(ABC)
```

Abstract base class for implementing animators.

<a id="lib.animator.Animator.tick"></a>

#### tick

```python
@abstractmethod
def tick(delta_time: float)
```

Advances the time for the animator by `delta_time`.

<a id="lib.animator.Animator.is_done"></a>

#### is\_done

```python
@abstractmethod
def is_done() -> bool
```

Return True if the animator is done playing the animation.

<a id="lib.animator.Animator.__eq__"></a>

#### \_\_eq\_\_

```python
@abstractmethod
def __eq__(other) -> bool
```

Return True if the animator is equal.

<a id="lib.animator.ValueAnimator"></a>

## ValueAnimator Objects

```python
class ValueAnimator(Animator)
```

Animates between equally distanced values over time.

