
#**************************************************************
# Create Clock
#**************************************************************

derive_pll_clocks -create_base_clocks

create_clock -name {ST_ALE} -period 10.000 [get_ports {ST_ALE}]
