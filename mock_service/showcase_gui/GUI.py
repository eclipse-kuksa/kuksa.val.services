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
)
from lib.trigger import ClockTrigger, EventTrigger, EventType
from lib.animator import RepeatMode
from lib.behavior import Behavior

class GUIElement:
    def __init__(self, name, vss_path, unit, datatype, read_only):
        self.name = name
        self.vss_path = vss_path
        self.unit = unit
        self.datatype = datatype
        self.read_only = read_only
        self.behaviors = []
        

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

        new_element = GUIElement(name, vss_path, unit, datatype, read_only)

        if datatype == DataType.BOOLEAN:
            element = self.create_toggle_button(new_element)
        elif datatype == DataType.STRING:
            label = ttk.Label(root, text=name+":")
            element = self.create_entry(new_element)
        else:
            label = ttk.Label(root, text=name+":")
            element, value_label = self.create_slider(new_element)

        if label is not None:
            self.elements.append(label)
        self.elements.append(element)
        if value_label is not None:
            self.elements.append(value_label)
        self.popup.destroy()

        if mock_datapoint:
            if metadata[vss_path].entry_type == EntryType.SENSOR:
                #only repeat mode available
                self.create_sensor_mock(new_element)
            elif metadata[vss_path].entry_type == EntryType.ACTUATOR:
                #add new behaviour
                self.create_new_behavior(new_element)
                pass
            else:
                messagebox.showerror("Error", f"VSS path is no Sensor or Actuator")
        self.update_layout()
        
    def actual_mock_sensor_datapoint(self, _duration, str_values, vss_path, datatype):
        str_values = str_values.split(',')
        _values = []
        for value in str_values:
            if not datatype == DataType.STRING or not datatype == DataType.STRING_ARRAY:
                value = float(value.strip())
            else:
                value = value.strip()
            _values.append(value) 
        
        mock_datapoint(
            path=vss_path,
            initial_value=0,
            behaviors=[
                create_behavior(
                    trigger=ClockTrigger(0),
                    action=create_animation_action(
                        duration=float(_duration),
                        repeat_mode=RepeatMode.REPEAT,
                        values=_values,
                    ),
                )
            ],
        )

        self.popup.destroy()

    def actual_mock_actuator_datapoint(self, element):           
        mock_datapoint(
            path=element.vss_path,
            initial_value=0,
            behaviors=element.behaviors,
        )

        self.popup.destroy()

    def show_sensor_popup(self, vss_path, datatype):
        
        self.popup = tk.Toplevel(root)
        self.popup.title("Specify Properties")
        self.popup.protocol("WM_DELETE_WINDOW", self.popup.destroy)
        
        # Create labels and entry fields for the properties in the self.popup window
        label1 = ttk.Label(self.popup, text="duration:")
        duration = tk.Entry(self.popup)

        label2 = ttk.Label(self.popup, text="comma separated list of values that are provided continously:")
        values = tk.Entry(self.popup)
        # Create a button to add the element within the self.popup window
        button = tk.Button(self.popup, text="Mock", command=lambda: app.actual_mock_sensor_datapoint(duration.get(), values.get(), vss_path, datatype))

        self.elements.append(label1)
        self.elements.append(duration)
        self.elements.append(label2)
        self.elements.append(values)
        self.elements.append(button)
        
        self.update_layout()

    def create_actuator_behavior(self, element, actionValue=None):
        if actionValue != "":
            if not element.datatype == DataType.STRING or not element.datatype == DataType.STRING_ARRAY:
                if element.datatype == DataType.FLOAT or element.datatype == DataType.DOUBLE or element.datatype == DataType.FLOAT_ARRAY or element.datatype == DataType.DOUBLE_ARRAY:
                    actionValue = float(actionValue)
                else:
                    actionValue = int(actionValue)
            action = create_set_action(actionValue)
        else: 
            action = create_set_action("$event.value")
        new_behavior = create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            action=action
        )
        element.behaviors.append(new_behavior)
        messagebox.showinfo(title="Info", message="Behavior created")

    def checksetToValueButton(self, toggle_var, setValue, label1):
        messagebox.showinfo(title="Info", message="You are creating a set action. This will lead to setting a fixed value if the target value is set. If you want that the target value is set leave the field empty. Otherwise provide a value")
        if toggle_var.get():
            last_entry = len(self.elements)
            # insert before buttons
            last_entry -= 2
            self.elements.insert(last_entry, label1)
            self.elements.insert(last_entry + 1, setValue)
            self.update_layout()
        else:
            if label1 in self.elements:
                index = self.elements.index(label1)
                self.elements[index].grid_forget()
                self.elements.remove(label1)

            if setValue in self.elements:
                index = self.elements.index(setValue)
                self.elements[index].grid_forget()
                self.elements.remove(setValue)
            self.update_layout()

    def checkRepeatButton(self, check):
        if check:
            return RepeatMode.REPEAT

    def show_actor_popup(self, element):
        
        self.popup = tk.Toplevel(root)
        self.popup.title("Specify Properties")
        self.popup.protocol("WM_DELETE_WINDOW", self.popup.destroy)

        label1 = ttk.Label(self.popup, text="set Value on event to:")
        setValue = tk.Entry(self.popup)

        toggle_var = tk.BooleanVar()
        setToValue = ttk.Checkbutton(
            self.popup,
            text="Set Datapoint to value",
            variable=toggle_var,
            command=lambda: self.checksetToValueButton(toggle_var, setValue, label1)
        )

        buttonBehavior = tk.Button(self.popup, text="Create behavior", command=lambda: self.create_actuator_behavior(element, setValue.get()))
        buttonAdd = tk.Button(self.popup, text="Mock", command=lambda: self.actual_mock_actuator_datapoint(element))

        self.elements.append(setToValue)
        self.elements.append(buttonBehavior)
        self.elements.append(buttonAdd)
        
        self.update_layout()

    def create_new_behavior(self, element):
        self.show_actor_popup(element)

    def create_sensor_mock(self, element):
        self.show_sensor_popup(element.vss_path, element.datatype)

    def update_datapoint(self, value, element):
        if not element.datatype == DataType.STRING or not element.datatype == DataType.STRING_ARRAY:
            value = float(value)
        if not element.read_only:
            self.client.set_current_values({element.vss_path: Datapoint(value)})

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
        entry1 = tk.Entry(self.popup)

        label2 = ttk.Label(self.popup, text="VSS path:")
        entry2 = tk.Entry(self.popup)

        checkbox_ro_var = tk.BooleanVar()
        checkbox = ttk.Checkbutton(self.popup, text="Read only", variable=checkbox_ro_var)

        checkbox_mock_var = tk.BooleanVar()
        checkbox_mock = ttk.Checkbutton(self.popup, text="Mock", variable=checkbox_mock_var)

        # Create a button to add the element within the self.popup window
        button = tk.Button(self.popup, text="Add", command=lambda: app.add_element(entry1.get(), entry2.get(), checkbox_ro_var.get(), checkbox_mock_var.get()))

        self.elements.append(label1)
        self.elements.append(entry1)
        self.elements.append(label2)
        self.elements.append(entry2)
        self.elements.append(checkbox)
        self.elements.append(checkbox_mock)
        self.elements.append(button)
        self.update_layout()

    def update_layout(self):
        # Clear existing elements
        for element in self.elements.copy(): # Create a copy of the list
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
    client = VSSClient("127.0.0.1", 55555)
    root = tk.Tk()
    app = GUIApp(client)
    app.mainloop()


