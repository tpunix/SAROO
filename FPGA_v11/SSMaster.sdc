
#**************************************************************
# Create Clock
#**************************************************************

derive_pll_clocks -create_base_clocks

create_clock -name {ST_ALE} -period 13.888 [get_ports {ST_ALE}]


