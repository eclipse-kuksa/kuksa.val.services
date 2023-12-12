# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
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

import argparse
import threading

import tkinter as tk
from tkinter import ttk
from tkinter import messagebox

from kuksa_client.grpc import VSSClient
from kuksa_client.grpc import VSSClientError
from kuksa_client.grpc import DataType
from kuksa_client.grpc import Datapoint
from kuksa_client.grpc import EntryType

from mock.mockservice import MockService
from lib.dsl import (
    create_animation_action,
    create_behavior,
    create_set_action,
    get_datapoint_value,
    mock_datapoint,
    create_event_trigger
)
from lib.trigger import ClockTrigger, EventType
from lib.animator import RepeatMode
from lib.behavior import Behavior


class GUIElement:
    def __init__(self, name, vss_path, metadata, read_only):
        self.name = name
        self.vss_path = vss_path
        self.read_only = read_only
        self.behaviors: list(Behavior) = []
        self.metadata = metadata


class GUIApp:
    def __init__(self, client):
        self.client = client
        self.client.connect()
        self.elements = []
        self.popup = tk.Toplevel(root)
        self.popup.destroy()

    def add_element(self, name: str, vss_path: str, read_only: bool, mock_datapoint: bool):
        # Get unit using kuksa client's get_metadata function
        try:
            metadata = self.client.get_metadata([vss_path,])
        except VSSClientError as e:
            messagebox.showerror("Error", e)
            return

        unit = metadata[vss_path].unit
        if unit is None:
            unit = ""

        datatype = metadata[vss_path].data_type

        value_label = None
        label = None

        new_element = GUIElement(name, vss_path, metadata[vss_path], read_only)

        if datatype == DataType.BOOLEAN:
            element = self.create_toggle_button(new_element)
        elif datatype == DataType.STRING:
            label = ttk.Label(root, text=name + ":")
            element = self.create_entry(new_element)
        else:
            label = ttk.Label(root, text=name + ":")
            element, value_label = self.create_slider(new_element)

        if label is not None:
            self.elements.append(label)
        self.elements.append(element)
        if value_label is not None:
            self.elements.append(value_label)
        self.popup.destroy()

        if mock_datapoint:
            self.create_new_behavior(new_element)
        self.update_layout()

    def actual_mock_datapoint(self, element):
        mock_datapoint(
            path=element.vss_path,
            initial_value=0,
            behaviors=element.behaviors,
        )

        self.popup.destroy()

    def create_mock_behavior(self, element, use_animation, use_set, setValue, cond_path, cond_val, clicked, str_values, duration, repeat, trigger_path, sec=0):
        if use_set or use_animation:
            if use_set:
                if setValue != "":
                    if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY or (setValue.startswith("$") and clicked != ClockTrigger):
                        action = create_set_action(setValue)
                    elif element.metadata.data_type == DataType.FLOAT or element.metadata.data_type == DataType.DOUBLE or element.metadata.data_type == DataType.FLOAT_ARRAY or element.metadata.data_type == DataType.DOUBLE_ARRAY:
                        action = create_set_action(float(setValue))
                    elif element.metadata.data_type == DataType.BOOLEAN or element.metadata.data_type == DataType.BOOLEAN_ARRAY:
                        action = create_set_action(bool(setValue))
                    else:
                        action = create_set_action(int(float(setValue)))
                else:
                    action = create_set_action("$event.value")
            elif use_animation:
                _repeat_mode = None
                if repeat:
                    _repeat_mode = RepeatMode.REPEAT

                if str_values != "":
                    str_values = str_values.split(',')
                    _values = []
                    for value in str_values:
                        value = value.strip()
                        if clicked != ClockTrigger:
                            if not value.startswith("$"):
                                if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY:
                                    pass
                                elif element.metadata.data_type == DataType.FLOAT or element.metadata.data_type == DataType.DOUBLE or element.metadata.data_type == DataType.FLOAT_ARRAY or element.metadata.data_type == DataType.DOUBLE_ARRAY:
                                    value = float(value)
                                elif element.metadata.data_type == DataType.BOOLEAN or element.metadata.data_type == DataType.BOOLEAN_ARRAY:
                                    value = bool(value)
                                else:
                                    value = int(float(value))
                        else:
                            if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY:
                                pass
                            elif element.metadata.data_type == DataType.FLOAT or element.metadata.data_type == DataType.DOUBLE or element.metadata.data_type == DataType.FLOAT_ARRAY or element.metadata.data_type == DataType.DOUBLE_ARRAY:
                                value = float(value)
                            elif element.metadata.data_type == DataType.BOOLEAN or element.metadata.data_type == DataType.BOOLEAN_ARRAY:
                                value = bool(value)
                            else:
                                value = int(float(value))
                        _values.append(value)

                    action = create_animation_action(_values, float(duration), _repeat_mode)
                else:
                    _Values = ["$self", "$event.value"]
                    action = create_animation_action(_Values, float(duration), _repeat_mode)

            if clicked == "ClockTrigger":
                _trigger = ClockTrigger(float(sec))
            elif clicked == "TargetTrigger":
                if trigger_path != "":
                    _trigger = create_event_trigger(EventType.ACTUATOR_TARGET, trigger_path)
                else:
                    _trigger = create_event_trigger(EventType.ACTUATOR_TARGET)
            elif clicked == "ValueTrigger":
                if trigger_path != "":
                    _trigger = create_event_trigger(EventType.VALUE, trigger_path)
                else:
                    _trigger = create_event_trigger(EventType.VALUE)

            if cond_val != "":
                if cond_path == "":
                    cond_path = element.vss_path
                    if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY or (setValue.startswith("$") and clicked != ClockTrigger):
                        pass
                    elif element.metadata.data_type == DataType.FLOAT or element.metadata.data_type == DataType.DOUBLE or element.metadata.data_type == DataType.FLOAT_ARRAY or element.metadata.data_type == DataType.DOUBLE_ARRAY:
                        cond_val = float(cond_val)
                    elif element.metadata.data_type == DataType.BOOLEAN or element.metadata.data_type == DataType.BOOLEAN_ARRAY:
                        cond_val = bool(cond_val)
                    else:
                        cond_val = int(float(cond_val))
                    new_behavior = create_behavior(
                        trigger=_trigger,
                        condition=lambda ctx: get_datapoint_value(
                            ctx, cond_path
                        ) == cond_val,
                        action=action
                    )
                else:
                    try:
                        metadata = self.client.get_metadata([cond_path,])[cond_path]
                        if metadata.data_type == DataType.STRING or metadata.data_type == DataType.STRING_ARRAY or (setValue.startswith("$") and clicked != ClockTrigger):
                            pass
                        elif metadata.data_type == DataType.FLOAT or metadata.data_type == DataType.DOUBLE or metadata.data_type == DataType.FLOAT_ARRAY or metadata.data_type == DataType.DOUBLE_ARRAY:
                            cond_val = float(cond_val)
                        elif metadata.data_type == DataType.BOOLEAN or metadata.data_type == DataType.BOOLEAN_ARRAY:
                            cond_val = bool(cond_val)
                        else:
                            cond_val = int(float(cond_val))

                        new_behavior = create_behavior(
                            trigger=_trigger,
                            condition=lambda ctx: get_datapoint_value(
                                ctx, cond_path
                            ) == cond_val,
                            action=action
                        )

                    except VSSClientError:
                        messagebox.showinfo(title="Info", message="The provided path does not exist.")
            else:
                new_behavior = create_behavior(
                    trigger=_trigger,
                    action=action
                )

            element.behaviors.append(new_behavior)
            messagebox.showinfo(title="Info", message="Behavior created")
        else:
            messagebox.showerror(title="Mock generation error", message="You need to choose one of mocking a datapoint through animation or setting to a fixed value")

    def checksetToValueButton(self, set_var, animation_var, setToValue_elements):
        if not animation_var.get():
            if set_var.get():
                messagebox.showinfo(title="Info", message="You are creating a set action. This will lead to setting a fixed value if the target value is set. If you want that the target value is set leave the field empty. Otherwise provide a value. If you do not want a condition leave it empty.")
                last_entry = len(self.elements)
                # insert before buttons
                last_entry -= 2
                counter = 0
                for element in setToValue_elements:
                    if element not in self.elements:
                        self.elements.insert(last_entry + counter, element)
                    counter += 1
                self.update_layout()
            else:
                for element in setToValue_elements:
                    if element in self.elements:
                        index = self.elements.index(element)
                        self.elements[index].grid_forget()
                        self.elements.remove(element)

                self.update_layout()
        else:
            set_var.set(0)

    def checkAnimationButton(self, animation_var, set_var, animationValue_elements):
        if not set_var.get():
            if animation_var.get():
                messagebox.showinfo(title="Info", message="You are creating an animation action. This will lead to setting a value through an animation with duration and some values. If you do not want a condition to be set leave it empty.")
                last_entry = len(self.elements)
                # insert before buttons
                last_entry -= 2
                counter = 0
                for element in animationValue_elements:
                    if element not in self.elements:
                        self.elements.insert(last_entry + counter, element)
                    counter += 1
                self.update_layout()
            else:
                for element in animationValue_elements:
                    if element in self.elements:
                        index = self.elements.index(element)
                        self.elements[index].grid_forget()
                        self.elements.remove(element)

                self.update_layout()
        else:
            animation_var.set(0)

    def showClockTriggerEntry(self, clicked, sec, label0, repeat, label1, trigger_path):
        last_entry = len(self.elements)
        # insert before buttons
        last_entry -= 2
        if clicked == "ClockTrigger":
            # add GUI elements for ClockTrigger
            if repeat not in self.elements:
                self.elements.insert(last_entry, repeat)
            if sec not in self.elements:
                self.elements.insert(last_entry, sec)
            if label0 not in self.elements:
                self.elements.insert(last_entry, label0)

            # remove GUI elements for TargetTrigger and ValueTrigger
            if label1 in self.elements:
                index = self.elements.index(label1)
                self.elements[index].grid_forget()
                self.elements.remove(label1)
            if trigger_path in self.elements:
                index = self.elements.index(trigger_path)
                self.elements[index].grid_forget()
                self.elements.remove(trigger_path)

            self.update_layout()
        else:
            # add GUI elements for TargetTrigger and ValueTrigger
            if trigger_path not in self.elements:
                self.elements.insert(last_entry, trigger_path)
            if label1 not in self.elements:
                self.elements.insert(last_entry, label1)

            # remove GUI elements for ClockTrigger
            if label0 in self.elements:
                index = self.elements.index(label0)
                self.elements[index].grid_forget()
                self.elements.remove(label0)
            if sec in self.elements:
                index = self.elements.index(sec)
                self.elements[index].grid_forget()
                self.elements.remove(sec)
            if repeat in self.elements:
                index = self.elements.index(repeat)
                self.elements[index].grid_forget()
                self.elements.remove(repeat)

            self.update_layout()

    def show_mock_popup(self, element):

        self.popup = tk.Toplevel(root)
        self.popup.title("Specify Properties")
        self.popup.protocol("WM_DELETE_WINDOW", self.popup.destroy)

        animation_elements = []
        setToValue_elements = []

        options = [
            "ClockTrigger",
            "ValueTrigger",
        ]
        if element.metadata.entry_type == EntryType.ACTUATOR and (element.metadata.data_type != DataType.STRING or element.metadata.data_type != DataType.STRING_ARRAY):
            options.append("TargetTrigger")

        label0 = ttk.Label(self.popup, text="clock trigger interval sec:")
        sec = tk.Entry(self.popup)

        repeat_var = tk.BooleanVar()
        repeat = ttk.Checkbutton(
            self.popup,
            text="Repeat mock continiously",
            variable=repeat_var,
        )

        label1 = ttk.Label(self.popup, text="VSS path the trigger acts on. if not specified it uses the the mocked VSS path:")
        trigger_path = tk.Entry(self.popup)
        # Create Dropdown
        # datatype of dropdown text
        clicked = tk.StringVar()

        # initial dropdown text
        clicked.set("ClockTrigger")

        drop = tk.OptionMenu(
            self.popup,
            clicked,
            *options,
            command=lambda clicked=clicked: self.showClockTriggerEntry(clicked, sec, label0, repeat, label1, trigger_path)
        )

        label2 = ttk.Label(self.popup, text="set Value on event to:")
        setValue = tk.Entry(self.popup)

        label3 = ttk.Label(self.popup, text="condition vss path:")
        cond_path = tk.Entry(self.popup)
        label4 = ttk.Label(self.popup, text="condition value:")
        cond_val = tk.Entry(self.popup)

        label5 = ttk.Label(self.popup, text="duration:")
        duration = tk.Entry(self.popup)
        label6 = ttk.Label(self.popup, text="comma separated list of values that are provided. Use $self for current value of sensor, $event.value for the value of the event trigger and $[vss path] for a VSS path:")
        values = tk.Entry(self.popup)

        setToValue_elements.append(label2)
        setToValue_elements.append(setValue)
        setToValue_elements.append(label3)
        setToValue_elements.append(cond_path)
        setToValue_elements.append(label4)
        setToValue_elements.append(cond_val)
        setToValue_elements.append(drop)

        animation_elements.append(label3)
        animation_elements.append(cond_path)
        animation_elements.append(label4)
        animation_elements.append(cond_val)
        animation_elements.append(label5)
        animation_elements.append(duration)
        animation_elements.append(label6)
        animation_elements.append(values)
        animation_elements.append(drop)

        animation_var = tk.BooleanVar()
        set_var = tk.BooleanVar()

        if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY:
            pass
        else:
            animationValue = ttk.Checkbutton(
                self.popup,
                text="Set Datapoint through animation",
                variable=animation_var,
                command=lambda: self.checkAnimationButton(animation_var, set_var, animation_elements)
            )
            self.elements.append(animationValue)

        setToValue = ttk.Checkbutton(
            self.popup,
            text="Set Datapoint to value",
            variable=set_var,
            command=lambda: self.checksetToValueButton(set_var, animation_var, setToValue_elements)
        )

        buttonBehavior = tk.Button(self.popup, text="Create behavior", command=lambda: self.create_mock_behavior(element, animation_var.get(), set_var.get(), setValue.get(), cond_path.get(), cond_val.get(), clicked.get(), values.get(), duration.get(), repeat_var.get(), trigger_path.get(), sec.get()))
        buttonAdd = tk.Button(self.popup, text="Mock", command=lambda: self.actual_mock_datapoint(element))

        self.elements.append(setToValue)
        self.elements.append(buttonBehavior)
        self.elements.append(buttonAdd)

        self.update_layout()

    def create_new_behavior(self, element):
        self.show_mock_popup(element)

    def update_datapoint(self, value, element):
        if value != "":
            if element.metadata.data_type == DataType.STRING or element.metadata.data_type == DataType.STRING_ARRAY:
                value = value
            elif element.metadata.data_type == DataType.FLOAT or element.metadata.data_type == DataType.DOUBLE or element.metadata.data_type == DataType.FLOAT_ARRAY or element.metadata.data_type == DataType.DOUBLE_ARRAY:
                value = float(value)
            elif element.metadata.data_type == DataType.BOOLEAN or element.metadata.data_type == DataType.BOOLEAN_ARRAY:
                value = bool(value)
            else:
                value = int(float(value))
            if not element.read_only:
                try:
                    self.client.set_current_values({element.vss_path: Datapoint(value)})
                except VSSClientError as e:
                    messagebox.showerror(f"{e} set initial value to avoid this.")

    def create_toggle_button(self, element):
        toggle_var = tk.BooleanVar()
        toggle_button = ttk.Checkbutton(
            root,
            text=element.name,
            variable=toggle_var,
            command=lambda: self.update_datapoint(toggle_var.get(), element)
        )
        setattr(toggle_button, 'name', element.vss_path)
        return toggle_button

    def create_slider(self, element):
        slider_var = tk.DoubleVar()
        slider = ttk.Scale(
            root,
            from_=0,
            to=1000,
            orient="horizontal",
            variable=slider_var,
            command=lambda value: self.update_datapoint(value, element)
        )
        # Assign a name to the slider using setattr()
        setattr(slider, 'name', element.vss_path)
        value_label = ttk.Label(root, textvariable=str(slider_var))
        return slider, value_label

    def create_entry(self, element):
        entry_var = tk.StringVar()
        entry_var.trace("w", lambda *args: self.update_datapoint(entry_var.get(), element))

        entry = ttk.Entry(root, textvariable=entry_var)
        setattr(entry, 'name', element.vss_path)
        return entry

    def show_popup(self):
        self.popup = tk.Toplevel(root)
        self.popup.title("Specify Properties")
        self.popup.protocol("WM_DELETE_WINDOW", self.popup.destroy)

        # Create labels and entry fields for the properties in the self.popup window
        label1 = ttk.Label(self.popup, text="name:")
        name = tk.Entry(self.popup)

        label2 = ttk.Label(self.popup, text="VSS path:")
        path = tk.Entry(self.popup)

        checkbox_ro_var = tk.BooleanVar()
        checkbox = ttk.Checkbutton(self.popup, text="Read only", variable=checkbox_ro_var)

        checkbox_mock_var = tk.BooleanVar()
        checkbox_mock = ttk.Checkbutton(self.popup, text="Mock", variable=checkbox_mock_var)

        # Create a button to add the element within the self.popup window
        button = tk.Button(self.popup, text="Add", command=lambda: app.add_element(name.get(), path.get(), checkbox_ro_var.get(), checkbox_mock_var.get()))

        self.elements.append(label1)
        self.elements.append(name)
        self.elements.append(label2)
        self.elements.append(path)
        self.elements.append(checkbox)
        self.elements.append(checkbox_mock)
        self.elements.append(button)
        self.update_layout()

    def update_layout(self):
        # Clear existing elements
        # Create a copy of the list
        for element in self.elements.copy():
            if element.winfo_exists():
                # Check if the widget exists
                element.grid_forget()
            else:
                self.elements.remove(element)  # Remove the destroyed widget from the list

        # Place elements in the layout
        for i, element in enumerate(self.elements):
            element.grid(row=i, column=0, padx=10, pady=5, sticky="w")

    def update_elements(self):
        for element in self.elements:
            if hasattr(element, 'name'):
                updated_value = self.client.get_current_values([str(element.name)])
                if updated_value[str(element.name)] is not None:
                    if isinstance(element, ttk.Checkbutton):
                        variable_name = element.cget("variable")
                        element.tk.globalsetvar(variable_name, int(updated_value[str(element.name)].value))
                    elif isinstance(element, ttk.Entry):
                        element.delete(0, tk.END)  # Clear the current contents of the Entry widget
                        element.insert(0, str(updated_value[str(element.name)].value))  # Insert the new value at position 0
                    elif isinstance(element, ttk.Scale):
                        element.set(updated_value[str(element.name)].value)
                    else:
                        print("Something wrong!")

    def mainloop(self):
        mock = MockService("127.0.0.1:50053")

        # GUI elements to add dynamically
        button = ttk.Button(root, text="Add Element", command=app.show_popup)
        app.elements.append(button)
        app.update_layout()
        Mock = threading.Thread(target=mock.main_loop)
        Mock.start()
        print("Mock started ...")
        while True:
            self.update_elements()

            # Process Tkinter events
            if root.winfo_exists():
                root.update()
                root.update_idletasks()
            else:
                break

            # Add a small delay to prevent high CPU usage
            root.after(10)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        usage="-a [BROKER ADDRESS] -p [BROKER PORT]",
        description="This UI interacts with a KUKSA.val databroker",
    )
    parser.add_argument("-a", "--address", default="127.0.0.1",
                        help="This indicates the address of the KUKSA.val databroker to connect to."
                        " The default value is 127.0.0.1")
    parser.add_argument("-p", "--port", default="55555",
                        help="This indicates the port of the kuksa.val databroker to connect to."
                        " The default value is 55555", type=int)
    args = parser.parse_args()
    client = VSSClient(args.address, args.port)
    root = tk.Tk()
    app = GUIApp(client)
    app.mainloop()
