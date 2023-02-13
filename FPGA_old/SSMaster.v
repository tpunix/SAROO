
///////////////////////////////////////////////////////
// Module: SEGA Saturn Master Flash Card             //
///////////////////////////////////////////////////////

// version 0.1: first step.

module SSMaster(
	// System
	CLK_50M,
	// SDRAM
	SD_CKE, SD_CLK, SD_CS, SD_WE, SD_CAS, SD_RAS, SD_ADDR, SD_BA, SD_DQM, SD_DQ,

	// SS system
	SS_MCLK, SS_RST,
	// SS I2S output
	SS_SCLK, SS_SSEL, SS_BCK, SS_LRCK, SS_SD,
	// SS ABUS
	SS_FC0, SS_FC1, SS_TIM0, SS_TIM1, SS_TIM2, SS_AAS,
	SS_ADDR, SS_DATA, SS_CS0, SS_CS1, SS_CS2, SS_RD, SS_WR0, SS_WR1, SS_WAIT, SS_IRQ,
	SS_DATA_OE, SS_DATA_DIR,

	// STM32 FSMC
	ST_CLK, ST_AD, ST_ADDR, ST_CS, ST_RD, ST_WR, ST_BL0, ST_BL1, ST_WAIT,
	// STM32 GPIO
	ST_GPIO0, ST_GPIO1, ST_GPIO2, ST_GPIO3,
	// STM32 I2S
	ST_MCLK, ST_BCK, ST_LRCK, ST_SDO,

	// DEBUG LED
	LED0, LED1
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// System
	input CLK_50M;

	// SDRAM
	output SD_CKE;
	output SD_CLK;
	output SD_CS;
	output SD_WE;
	output SD_CAS;
	output SD_RAS;
	output[13:0] SD_ADDR;
	output[ 1:0] SD_BA;
	output[ 1:0] SD_DQM;
	inout[15:0] SD_DQ;

	// SS System
	input SS_MCLK;
	input SS_RST;

	// SS I2S
	input SS_SCLK;
	output SS_SSEL;
	output SS_BCK;
	output SS_LRCK;
	output SS_SD;

	// SS ABUS
	input SS_FC0;
	input SS_FC1;
	input SS_TIM0;
	input SS_TIM1;
	input SS_TIM2;
	input SS_AAS;
	input[23:0] SS_ADDR;
	inout[15:0] SS_DATA;
	input SS_CS0;
	input SS_CS1;
	input SS_CS2;
	input SS_RD;
	input SS_WR0;
	input SS_WR1;
	output SS_WAIT;
	output SS_IRQ;
	output SS_DATA_OE;
	output SS_DATA_DIR;

	// STM32 FSMC
	input ST_CLK;
	inout[15:0] ST_AD;
	input[ 7:0] ST_ADDR;
	input ST_CS;
	input ST_RD;
	input ST_WR;
	input ST_BL0;
	input ST_BL1;
	output ST_WAIT;
	
	// STM32 GPIO
	input ST_GPIO0;
	input ST_GPIO1;
	output ST_GPIO2;
	input ST_GPIO3;

	// STM32 I2S
	output ST_MCLK;
	input ST_BCK;
	input ST_LRCK;
	input ST_SDO;

	// DEBUG LED
	output LED0;
	output LED1;

///////////////////////////////////////////////////////
// Debug LED                                         //
///////////////////////////////////////////////////////

	assign LED0 = SS_WAIT;
	assign LED1 = ss_reg_ctrl[8];

	wire NRESET = ST_GPIO0;
	wire ST_ALE = ST_GPIO1;
	assign ST_GPIO2 = ST_IRQ;

///////////////////////////////////////////////////////
// I2S                                               //
///////////////////////////////////////////////////////

	assign ST_MCLK = SS_SCLK;
	assign SS_BCK  = ST_BCK;
	assign SS_LRCK = ST_LRCK;
	assign SS_SD   = ST_SDO;

///////////////////////////////////////////////////////
// STM32 FSMC                                        //
///////////////////////////////////////////////////////

	// fsmc address latch
	reg[24:0] fsmc_addr;
	reg fsmc_cs;

	always @(posedge ST_ALE)
	begin
		fsmc_addr <= {ST_ADDR, ST_AD, 1'b0};
	end

	always @(posedge ST_CS or posedge ST_ALE)
	begin
		if(ST_CS==1)
			fsmc_cs <= 1;
		else
			fsmc_cs <= 0;
	end

	// fsmc read sync
	reg stale_s0, stale_s1, stale_s2;

	always @(posedge avm_clk)
	begin
		stale_s0 <= ST_ALE;
		stale_s1 <= stale_s0;
		stale_s2 <= stale_s1;
	end
	// rising edge of ale
	wire st_rd_start = (stale_s2==0 && stale_s1==1 && ST_WR==1);


	// fsmc write sync
	reg stnwr_s0, stnwr_s1, stnwr_s2;

	always @(posedge avm_clk)
	begin
		stnwr_s0 <= ST_WR;
		stnwr_s1 <= stnwr_s0;
		stnwr_s2 <= stnwr_s1;
	end
	wire st_wr_start = (stnwr_s2==1 && stnwr_s1==0); // falling edge of wr

///////////////////////////////////////////////////////
// STM32 FPGA control register                       //
///////////////////////////////////////////////////////

	reg[15:0] st_reg_ctrl;
	reg[15:0] ss_resp1;
	reg[15:0] ss_resp2;
	reg[15:0] ss_resp3;
	reg[15:0] ss_resp4;
	
	reg[31:0] st_reg_blk_addr;
	reg[15:0] st_reg_blk_size;
	reg[15:0] st_reg_fifo_ctrl;

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			st_reg_ctrl <= 0;
		end else if(st_wr_start==1) begin
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h04) st_reg_ctrl <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h08) st_reg_blk_addr[15:0] <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h0a) st_reg_blk_addr[31:16] <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h0c) st_reg_blk_size <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h10) st_reg_fifo_ctrl <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h20) ss_resp1 <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h22) ss_resp2 <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h24) ss_resp3 <= ST_AD;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h26) ss_resp4 <= ST_AD;
		end
	end



///////////////////////////////////////////////////////
// Saturn to STM32: HIRQ register                    //
///////////////////////////////////////////////////////

	reg[15:0] ss_hirq;


	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			ss_hirq <= 0;
		end else begin
			if(     st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h28)
				ss_hirq <= ss_hirq|ST_AD;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h16)
				ss_hirq <= ss_hirq&(~ST_AD);
			else if(ss_wr_start==1 && ss_cdc_cs==1 && SS_ADDR[5:2]==4'b00_10)
				ss_hirq <= ss_hirq&SS_DATA;
		end
	end


///////////////////////////////////////////////////////
// Saturn to STM32: CDC request                      //
///////////////////////////////////////////////////////

	reg st_irq_cdc;

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			st_irq_cdc <= 1'b0;
		end else begin
			if(ss_wr_start==1 && ss_cdc_cs==1 && SS_ADDR[5:2]==4'b10_01)
				st_irq_cdc <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_cdc <= st_irq_cdc&(~ST_AD[0]);
		end
	end

///////////////////////////////////////////////////////
// Saturn to STM32: CMD request                      //
///////////////////////////////////////////////////////

	reg st_irq_cmd;

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			st_irq_cmd <= 1'b0;
		end else begin
			if(ss_wr_start==1 && ss_reg_cs==1 && SS_ADDR[5:1]==5'b00_110)
				st_irq_cmd <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_cmd <= st_irq_cmd&(~ST_AD[1]);
		end
	end

///////////////////////////////////////////////////////
// CDC FIFO dma end                                  //
///////////////////////////////////////////////////////

	reg st_irq_fifo;

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			st_irq_fifo <= 1'b0;
		end else begin
			if(fifo_blk_dma_end==1)
				st_irq_fifo <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_fifo <= st_irq_fifo&(~ST_AD[2]);
		end
	end

///////////////////////////////////////////////////////
// Saturn to STM32: CR4 read                         //
///////////////////////////////////////////////////////

	reg st_irq_cr4rd;

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0) begin
			st_irq_cr4rd <= 1'b0;
		end else begin
			if(ss_rd_start==1 && ss_cdc_cs==1 && SS_ADDR[5:2]==4'b10_01)
				st_irq_cr4rd <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_cr4rd <= st_irq_cr4rd&(~ST_AD[3]);
		end
	end

///////////////////////////////////////////////////////
// STM32 read data                                   //
///////////////////////////////////////////////////////

	reg[15:0] st_reg_data_out;

	always @(posedge avm_clk)
	begin
		st_reg_data_out <= 
						(fsmc_addr[7:0]==8'h00)? 16'h5253 : // ID: "SR"
						(fsmc_addr[7:0]==8'h02)? 16'h1101 : // ver: HW1.1 && SW0.1
						(fsmc_addr[7:0]==8'h04)? st_reg_ctrl :
						(fsmc_addr[7:0]==8'h06)? st_reg_stat :
						(fsmc_addr[7:0]==8'h08)? st_reg_blk_addr[15: 0] :
						(fsmc_addr[7:0]==8'h0a)? st_reg_blk_addr[31:16] :
						(fsmc_addr[7:0]==8'h0c)? st_reg_blk_size :
						(fsmc_addr[7:0]==8'h10)? st_reg_fifo_ctrl :
						(fsmc_addr[7:0]==8'h12)? st_reg_fifo_stat :
						(fsmc_addr[7:0]==8'h14)? ss_reg_ctrl :

						(fsmc_addr[7:0]==8'h18)? ss_resp1 :
						(fsmc_addr[7:0]==8'h1a)? ss_resp2 :
						(fsmc_addr[7:0]==8'h1c)? ss_resp3 :
						(fsmc_addr[7:0]==8'h1e)? ss_resp4 :

						(fsmc_addr[7:0]==8'h20)? ss_cr1 :
						(fsmc_addr[7:0]==8'h22)? ss_cr2 :
						(fsmc_addr[7:0]==8'h24)? ss_cr3 :
						(fsmc_addr[7:0]==8'h26)? ss_cr4 :
						(fsmc_addr[7:0]==8'h28)? ss_hirq :
						(fsmc_addr[7:0]==8'h2a)? ss_hirq_mask :
						(fsmc_addr[7:0]==8'h2c)? ss_mrgb :
						(fsmc_addr[7:0]==8'h2e)? ss_reg_cmd :

						(fsmc_addr[7:0]==8'h30)? fifo_rd_times[15: 0] :
						(fsmc_addr[7:0]==8'h32)? fifo_rd_times[31:16] :
						16'hffff;
	end

	assign ST_AD = (ST_RD==0 && ST_CS==0)? (
						(fsmc_addr[24]==0)? st_reg_data_out : st_ram_data_out
					) : 16'hzzzz;

	wire st_ram_cs = !(fsmc_cs==0 && ST_ADDR[7]==1);

	wire[15:0] st_reg_stat;
	assign st_reg_stat = {pll_locked, ST_IRQ, 10'b0, st_irq_cr4rd, st_irq_fifo, st_irq_cmd, st_irq_cdc};

	wire st_irq_cr4en  = st_reg_ctrl[3];
	wire st_irq_fifoen = st_reg_ctrl[2];
	wire st_irq_cmd_en = st_reg_ctrl[1];
	wire st_irq_cdc_en = st_reg_ctrl[0];

	wire ST_IRQ = ( (st_irq_cdc_en==1 && st_irq_cdc==1) ||
					(st_irq_cmd_en==1 && st_irq_cmd==1) ||
					(st_irq_cr4en ==1 && st_irq_cr4rd==1) ||
					(st_irq_fifoen==1 && st_irq_fifo==1) );

///////////////////////////////////////////////////////
// SATURN ABUS                                       //
///////////////////////////////////////////////////////

	reg sscs_s0, sscs_s1, sscs_s2, sscs_s3, sscs_s4;

	always @(posedge avm_clk)
	begin
		sscs_s0 <= (SS_CS0 & SS_CS1 & SS_CS2);
		sscs_s1 <= sscs_s0;
		sscs_s2 <= sscs_s1;
		sscs_s3 <= sscs_s2;
		sscs_s4 <= sscs_s3;
	end

	wire ss_rd_start = (sscs_s2==1 && sscs_s1==0 && SS_RD==0);
	wire ss_wr_start = (sscs_s4==1 && sscs_s3==0 && (SS_WR0==0 || SS_WR1==0));

	assign SS_DATA =(SS_RD==0 && SS_CS0==0)? ss_ram_data_out :
					(SS_RD==0 && SS_CS1==0)? ss_cs1_data_out :
					(SS_RD==0 && ss_reg_cs==1)? ss_bcr_data_out :
					(SS_RD==0 && ss_cdc_cs==1)? ss_cdc_data_out :
					16'hzzzz;

	assign SS_DATA_OE = (SS_CS0==1 && SS_CS1==1 && (SS_CS2==1 || (ss_cdc_cs==1 && SS_RD==0 && ss_cdc_en==0)));

	assign SS_DATA_DIR = (SS_WR0 & SS_WR1);

///////////////////////////////////////////////////////
// SATURN System Control                             //
///////////////////////////////////////////////////////

	reg[15:0] ss_bcr_data_out;
	reg[15:0] ss_reg_ctrl;
	reg[15:0] ss_reg_cmd;
	reg[31:0] ss_reg_timer;

	// 00: None
	// 01: Data Cart
	// 10: RAM Cart: 1MBytes 
	// 11: RAM Cart: 4MBytes 
	wire[1:0] ss_cs0_type = ss_reg_ctrl[13:12];

	wire ss_reg_cs = (SS_CS2==0 && SS_ADDR[14:12]==3'b111);
	
	wire[15:0] ss_cs1_data_out = (SS_ADDR[23:16]==8'hff && ss_cs0_type[1]==1)?
									(
										(ss_cs0_type[0]==0)? 16'hff5a : 16'hff5c
									) : ss_ram_data_out;

	always @(posedge avm_clk)
	begin
		ss_bcr_data_out <= 
			(SS_ADDR[5:1]==5'b00_000)? 16'h5253 : // ID: "SR"
			(SS_ADDR[5:1]==5'b00_001)? 16'h1101 : // ver: HW1.1 && SW0.1
			(SS_ADDR[5:1]==5'b00_010)? ss_reg_ctrl :
			(SS_ADDR[5:1]==5'b00_100)? ss_reg_timer[31:16] :
			(SS_ADDR[5:1]==5'b00_101)? ss_reg_timer[15: 0] :
			(SS_ADDR[5:1]==5'b00_110)? ss_reg_cmd :
			16'h0000;
	end


	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0)
			ss_reg_ctrl <= 16'h0100;
		else if(ss_wr_start==1 && ss_reg_cs==1 && SS_ADDR[5:1]==5'b00_010)
			ss_reg_ctrl <= SS_DATA;
	end

	always @(negedge NRESET or posedge avm_clk)
	begin
		if(NRESET==0)
			ss_reg_cmd <= 4'b0000;
		else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h2e)
			ss_reg_cmd <= 0;
		else if(ss_wr_start==1 && ss_reg_cs==1 && SS_ADDR[5:1]==5'b00_110)
			ss_reg_cmd <= SS_DATA;
	end


	reg[6:0] ss_timer_feed;

	always @(posedge avm_clk)
	begin
		if(ss_timer_feed==7'd99)
			ss_timer_feed <= 0;
		else
			ss_timer_feed <= ss_timer_feed+7'b1;
	end

	always @(posedge avm_clk)
	begin
		if(ss_wr_start==1 && ss_reg_cs==1 && SS_ADDR[5:1]==5'b00_100)
			ss_reg_timer <= 0;
		else if(ss_timer_feed==0) begin
			ss_reg_timer <= ss_reg_timer+32'b1;
		end
	end


///////////////////////////////////////////////////////
// SATURN CDC                                        //
///////////////////////////////////////////////////////

	reg[15:0] ss_cdc_data_out;
	reg[15:0] ss_hirq_mask;
	reg[15:0] ss_cr1;
	reg[15:0] ss_cr2;
	reg[15:0] ss_cr3;
	reg[15:0] ss_cr4;
	reg[15:0] ss_mrgb;

	// enable CDC read out
	wire ss_cdc_en = ss_reg_ctrl[15];

	wire ss_cdc_cs = (SS_CS2==0 && SS_ADDR[14:12]==3'b000);
	wire ss_cdc_data = (ss_cdc_cs==1 && SS_ADDR[5:2]==0);

	always @(posedge avm_clk)
	begin
		if(ss_wr_start==1 && ss_cdc_cs==1) begin
			if(SS_ADDR[5:2]==4'b00_11) ss_hirq_mask <= SS_DATA;
			if(SS_ADDR[5:2]==4'b01_10) ss_cr1   <= SS_DATA;
			if(SS_ADDR[5:2]==4'b01_11) ss_cr2   <= SS_DATA;
			if(SS_ADDR[5:2]==4'b10_00) ss_cr3   <= SS_DATA;
			if(SS_ADDR[5:2]==4'b10_01) ss_cr4   <= SS_DATA;
			if(SS_ADDR[5:2]==4'b10_10) ss_mrgb  <= SS_DATA;
		end
	end

	always @(posedge avm_clk)
	begin
		ss_cdc_data_out <= 
			(SS_ADDR[5:2]==4'b00_00)? ss_fifo_data_out :
			(SS_ADDR[5:2]==4'b00_10)? ss_hirq :
			(SS_ADDR[5:2]==4'b00_11)? ss_hirq_mask :
			(SS_ADDR[5:2]==4'b01_10)? ss_resp1 :
			(SS_ADDR[5:2]==4'b01_11)? ss_resp2 :
			(SS_ADDR[5:2]==4'b10_00)? ss_resp3 :
			(SS_ADDR[5:2]==4'b10_01)? ss_resp4 :
			(SS_ADDR[5:2]==4'b10_10)? ss_mrgb :
			16'h0000;
	end

	assign SS_SSEL = ~ss_cdc_en;

	assign SS_IRQ = (ss_hirq&ss_hirq_mask)==0? 1'b0: 1'b1;


///////////////////////////////////////////////////////////////////////////////////////////////////


    // FIFO test
	reg[31:0] fifo_rd_times;

	always @(posedge avm_clk)
	begin
		if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h30)
			fifo_rd_times <= 0;
		else if(ss_rd_start==1 && ss_cdc_data==1) begin
			fifo_rd_times <= fifo_rd_times+32'b1;
		end
	end



///////////////////////////////////////////////////////
// QSYS                                              //
///////////////////////////////////////////////////////

	wire avm_clk;
	wire pll_locked;
	wire[15:0] st_ram_data_out;

	wire[15:0] ss_ram_din = {SS_DATA[7:0], SS_DATA[15:8]};
	wire[15:0] ss_ram_dout;
	wire[15:0] ss_ram_data_out = {ss_ram_dout[7:0], ss_ram_dout[15:8]};
	wire[15:0] ss_fifo_dout;
	wire[15:0] ss_fifo_data_out = {ss_fifo_dout[7:0], ss_fifo_dout[15:8]};

	wire[15:0] st_reg_fifo_stat;
	wire fifo_blk_dma_end;

	cqsys u0 (
		.clk_clk       (CLK_50M),
		.reset_reset_n (NRESET),
		.avm_clk_clk   (avm_clk),

		.mem_pin_addr  (SD_ADDR),
		.mem_pin_ba    (SD_BA),
		.mem_pin_cas_n (SD_CAS),
		.mem_pin_cke   (SD_CKE),
		.mem_pin_cs_n  (SD_CS),
		.mem_pin_dq    (SD_DQ),
		.mem_pin_dqm   (SD_DQM),
		.mem_pin_ras_n (SD_RAS),
		.mem_pin_we_n  (SD_WE),

		.fsmc_bus_addr        ({8'b0, fsmc_addr[23:0]}),
		.fsmc_bus_ncs         (st_ram_cs),
		.fsmc_bus_rd_start    (st_rd_start),
		.fsmc_bus_wr_start    (st_wr_start),
		.fsmc_bus_byte_en     ({~ST_BL1, ~ST_BL0}),
		.fsmc_bus_data_in     (ST_AD),
		.fsmc_bus_data_out    (st_ram_data_out),
		.fsmc_bus_wait_out    (ST_WAIT),

        .saturn_bus_addr      ({7'b0, ~SS_CS1, SS_ADDR}),
        .saturn_bus_ncs       (SS_CS0&SS_CS1),
        .saturn_bus_rd_start  (ss_rd_start),
        .saturn_bus_wr_start  (ss_wr_start),
		.saturn_bus_byte_en   ({~SS_WR0, ~SS_WR1}),
        .saturn_bus_data_in   (ss_ram_din),
        .saturn_bus_data_out  (ss_ram_dout),
        .saturn_bus_wait_out  (SS_WAIT),

        .cdc_fifo_reg_fifo_ctrl (st_reg_fifo_ctrl),
        .cdc_fifo_reg_fifo_stat (st_reg_fifo_stat),
        .cdc_fifo_reg_blk_addr  (st_reg_blk_addr),
        .cdc_fifo_reg_blk_size  (st_reg_blk_size),
        .cdc_fifo_rd_start      ((ss_rd_start==1 && ss_cdc_data==1)),
        .cdc_fifo_data_out      (ss_fifo_dout),
        .cdc_fifo_blk_dma_end   (fifo_blk_dma_end),

		.altpll_locked_export (pll_locked)
	);

	assign SD_CLK = avm_clk;

endmodule
