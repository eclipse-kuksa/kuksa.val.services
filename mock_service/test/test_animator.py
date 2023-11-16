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

from lib.animator import RepeatMode, ValueAnimator


def test_value_animator_no_repeat():
    anim = ValueAnimator([0.0, 10.0], 10.0, RepeatMode.ONCE)
    v0 = anim.get_value()
    anim.tick(5.0)
    v1 = anim.get_value()
    anim.tick(5.0)
    v2 = anim.get_value()
    anim.tick(5.0)
    v3 = anim.get_value()

    assert v0 == 0.0
    assert v1 == 5.0
    assert v2 == 10.0
    assert v3 == 10.0
    assert anim.is_done()


def test_value_animator_repeat():
    anim = ValueAnimator([0.0, 100.0], 10.0, RepeatMode.REPEAT)
    v0 = anim.get_value()
    anim.tick(5.0)
    v1 = anim.get_value()
    anim.tick(5.0)
    v2 = anim.get_value()
    anim.tick(2.0)
    v3 = anim.get_value()

    assert v0 == 0.0
    assert v1 == 50.0
    assert v2 == 100.0
    assert v3 == 20.0
    assert not anim.is_done()
