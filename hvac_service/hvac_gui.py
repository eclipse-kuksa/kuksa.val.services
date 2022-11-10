import dearpygui.dearpygui as dpg
import os
import testclient

def main():
    hvac_addr = os.getenv("HVAC_ADDR", "127.0.0.1:50052")
    client = testclient.HVACTestClient(hvac_addr)
    count = 0

    dpg.create_context()
    dpg.create_viewport(title='HVAC service demo', width=600, height=600)
    dpg.setup_dearpygui()

    def OnOffButton(sender):
        item = dpg.get_item_configuration("ToggleButton")
        if item['label'] == 'OFF':
            dpg.configure_item("ToggleButton", label='ON')
            status = 1
        else:
            dpg.configure_item("ToggleButton", label='OFF')
            status = 0
        client.setAC(status)

    def slideTemp(sender):
        value = dpg.get_value("TempSlider")
        # setting is performed here becasue the current temperature is set with this
        client.setTemp(value)

    with dpg.window(label="Demo Window"):
        dpg.add_slider_int(tag="TempSlider", label="current temperature in celsius", default_value=1, vertical=True, max_value=40, height=160, callback=slideTemp)
        dpg.add_slider_int(tag="TargetTempSlider", label="target temperature in celsius", default_value=20, vertical=True, max_value=40, height=160)
        dpg.add_button(tag="ToggleButton", label="OFF", callback=OnOffButton)
        dpg.add_checkbox(tag="SetAutomation", label="automate", default_value=True)
        dpg.add_input_int(tag="TempRatio", label="Ratio to change temperature", default_value=30)
        dpg.add_input_int(tag="SensorRatio", label="Ratio to poll sensor", default_value=10)

    dpg.show_viewport()
    while dpg.is_dearpygui_running():
        item = dpg.get_item_configuration("ToggleButton")
        value = dpg.get_value("TempSlider")
        automate = dpg.get_value("SetAutomation")
        target = dpg.get_value("TargetTempSlider")
        temp_ratio = dpg.get_value("TempRatio")
        sensor_ratio = dpg.get_value("SensorRatio")

        if (count % temp_ratio) == 0:
            if item['label'] == 'OFF':
                dpg.set_value("TempSlider", value + 1)
            elif item['label'] == 'ON':
                dpg.set_value("TempSlider", value - 1)
            count = 1
                
        # setAC is in the if loop because unlike a sensor a button responds immediately
        if automate:
            if value >= target:
                dpg.configure_item("ToggleButton", label='ON')
                client.setAC(1)
            elif value < target:
                dpg.configure_item("ToggleButton", label='OFF')
                client.setAC(0)
            
        #prevent crashing
        if value == 40 or value == -18:
            dpg.set_value("TempSlider", 15)

        # mock integrated temperature sensor
        if (count % sensor_ratio) == 0:
            client.setTemp(value)
        
        dpg.render_dearpygui_frame()

        count+=1

    dpg.destroy_context()

if __name__ == '__main__':
    main()