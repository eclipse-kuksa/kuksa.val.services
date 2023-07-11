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
import sys

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
    _mocked_datapoints
)
from lib.trigger import ClockTrigger, EventTrigger, EventType
from lib.animator import RepeatMode

class GUIApp:
    def __init__(self, client):
        self.client = client
        self.client.connect()
        self.elements = []
        self.popup = tk.Toplevel(root)
        self.popup.destroy()

    def add_element(self, name, vss_path: str, read_only: bool, mock_datapoint: bool):
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

        if datatype == DataType.BOOLEAN:
            element = self.create_toggle_button(name, vss_path, read_only, unit, datatype)
        elif datatype == DataType.STRING:
            label = ttk.Label(root, text=name+":")
            element = self.create_entry(name, vss_path, read_only, unit, datatype)
        else:
            label = ttk.Label(root, text=name+":")
            element, value_label = self.create_slider(vss_path, read_only, unit, datatype)

        if label is not None:
            self.elements.append(label)
        self.elements.append(element)
        if value_label is not None:
            self.elements.append(value_label)
        self.popup.destroy()

        if mock_datapoint:
            if metadata[vss_path].entry_type == EntryType.SENSOR:
                #only repeat mode available
                self.create_sensor_mock(vss_path, datatype)
            elif metadata[vss_path].entry_type == EntryType.ACTUATOR:
                #add new behaviour
                pass
            else:
                messagebox.showerror("Error", f"VSS path is no Sensor or Actuator")
        self.update_layout()
        

    def actual_mock_datapoint(self, _duration, str_values, vss_path, datatype):
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
        button = tk.Button(self.popup, text="Mock", command=lambda: app.actual_mock_datapoint(duration.get(), values.get(), vss_path, datatype))

        self.elements.append(label1)
        self.elements.append(duration)
        self.elements.append(label2)
        self.elements.append(values)
        self.elements.append(button)
        
        self.update_layout()

    def create_sensor_mock(self, vss_path, datatype):
        self.show_sensor_popup(vss_path, datatype)

    def update_datapoint(self, value, vss_path, read_only, datatype):
        if not datatype == DataType.STRING or not datatype == DataType.STRING_ARRAY:
            value = float(value)
        if not read_only:
            self.client.set_current_values({vss_path: Datapoint(value)})

    def create_toggle_button(self, name, vss_path, read_only, unit, datatype):
        toggle_var = tk.BooleanVar()
        toggle_button = ttk.Checkbutton(
            root,
            text=name,
            variable=toggle_var,
            command=lambda: self.update_datapoint(toggle_var.get(), vss_path, read_only, datatype)
        )
        setattr(toggle_button, 'name', vss_path)
        return toggle_button

    def create_slider(self, vss_path, read_only, unit, datatype):
        slider_var = tk.DoubleVar()
        slider = ttk.Scale(
            root,
            from_=0,
            to=1000,
            orient="horizontal",
            variable=slider_var,
            command=lambda value: self.update_datapoint(value, vss_path, read_only, datatype)
        )
        # Assign a name to the slider using setattr()
        setattr(slider, 'name', vss_path)
        value_label = ttk.Label(root, textvariable=str(slider_var))
        return slider, value_label

    def create_entry(self, name, vss_path, read_only, unit, datatype):
        entry_var = tk.StringVar()
        entry_var.trace("w", lambda *args: self.update_datapoint(entry_var.get(), vss_path, read_only, datatype))
        
        entry = ttk.Entry(root, textvariable=entry_var)
        setattr(entry, 'name', vss_path)
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
        for element in self.elements.copy():  # Create a copy of the list
            if element.winfo_exists():  # Check if the widget exists
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


