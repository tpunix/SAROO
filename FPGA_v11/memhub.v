
///////////////////////////////////////////////////////
// Module: Dual Master Bus system                    //
///////////////////////////////////////////////////////

module memhub(
	reset, clk,
	cs_a, rd_a, wr_a, mask_a, nwait_a, addr_a, wdata_a, rdata_a,
	cs_b, rd_b, wr_b, mask_b, nwait_b, addr_b, wdata_b, rdata_b,
	sd_cke, sd_cs, sd_ras, sd_cas, sd_we, sd_addr, sd_ba, sd_dqm, sd_data,
	ext_refresh
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// system
	input reset;
	input clk;

	// bus a
	input cs_a;
	input rd_a;
	input wr_a;
	input[1:0] mask_a;
	output nwait_a;
	input[25:0] addr_a;
	input[15:0] wdata_a;
	output[15:0] rdata_a;

	// bus b
	input cs_b;
	input rd_b;
	input wr_b;
	input[1:0] mask_b;
	output nwait_b;
	input[25:0] addr_b;
	input[15:0] wdata_b;
	output[15:0] rdata_b;

	// sdram
	output sd_cke;
	output sd_cs;
	output sd_ras;
	output sd_cas;
	output sd_we;
	output[12:0] sd_addr;
	output[ 1:0] sd_ba;
	output[ 1:0] sd_dqm;
	inout [15:0] sd_data;
	input ext_refresh;


///////////////////////////////////////////////////////
// bus A                                             //
///////////////////////////////////////////////////////

	wire[ 1:0] cmd_req_a;
	wire cache_invalid_a;
	wire cache_update_a;

	cachebus busa(reset, clk,
		cs_a, rd_a, wr_a, nwait_a, addr_a, wdata_a, rdata_a,
		cmd_req_a, cmd_ack_a, cache_invalid_a, cache_update_a,
		cache_addr_a, cache_data1d_a, cache_valid_a
	);

	wire[25:3] cache_addr_a;
	wire[63:0] cache_data1d_a;
	wire[ 3:0] cache_valid_a;
	wire data_valid_a = (data_valid==1 && data_ab==0);

	cacheblk _cha(reset, clk,
		cache_addr_a, cache_data1d_a, cache_valid_a, data_valid_a, cmd_dout,
		cache_update_a, mask_a, addr_a, wdata_a,
		cache_update_b, mask_b, addr_b, wdata_b,
		cache_invalid_a, addr_a
	);


///////////////////////////////////////////////////////
// bus B                                             //
///////////////////////////////////////////////////////

	wire[ 1:0] cmd_req_b;
	wire cache_invalid_b;
	wire cache_update_b;

	cachebus busb(reset, clk,
		cs_b, rd_b, wr_b, nwait_b, addr_b, wdata_b, rdata_b,
		cmd_req_b, cmd_ack_b, cache_invalid_b, cache_update_b,
		cache_addr_b, cache_data1d_b, cache_valid_b
	);


	wire[25:3] cache_addr_b;
	wire[63:0] cache_data1d_b;
	wire[ 3:0] cache_valid_b;
	wire data_valid_b = (data_valid==1 && data_ab==1);

	cacheblk _chb(reset, clk,
		cache_addr_b, cache_data1d_b, cache_valid_b, data_valid_b, cmd_dout,
		cache_update_b, mask_b, addr_b, wdata_b,
		cache_update_a, mask_a, addr_a, wdata_a,
		cache_invalid_b, addr_b
	);


///////////////////////////////////////////////////////
// request manager                                   //
///////////////////////////////////////////////////////

	reg[1:0] state;
	reg[ 1:0] cmd_req;
	reg[ 1:0] cmd_mask;
	reg[25:0] cmd_addr;
	reg[15:0] cmd_din;
	reg act_ab;
	reg data_ab;
	reg cmd_ack_a;
	reg cmd_ack_b;


	localparam C_IDLE=0, C_WAITACK=1, C_END=2;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			state <= C_IDLE;
			cmd_req <= 0;
		end else begin
			case(state)
			C_IDLE: begin
				if(cmd_req_a) begin
					act_ab <= 1'b0;
					cmd_req <= cmd_req_a;
					cmd_mask <= mask_a;
					cmd_addr[25:3] <= addr_a[25:3];
					cmd_addr[2:0] <= (cmd_req_a[0])? addr_a[2:0] : 3'b0;
					cmd_din <= wdata_a;
					state <= C_WAITACK;
				end else if(cmd_req_b) begin
					act_ab <= 1'b1;
					cmd_req <= cmd_req_b;
					cmd_mask <= mask_b;
					cmd_addr[25:3] <= addr_b[25:3];
					cmd_addr[2:0] <= (cmd_req_b[0])? addr_b[2:0] : 3'b0;
					cmd_din <= wdata_b;
					state <= C_WAITACK;
				end
			end

			C_WAITACK: begin
				if(cmd_ack) begin
					cmd_ack_a <= (act_ab==0);
					cmd_ack_b <= (act_ab==1);
					data_ab <= act_ab;
					cmd_req <= 0;
					state <= C_END;
				end
			end

			C_END: begin
				cmd_ack_a <= 0;
				cmd_ack_b <= 0;
				state <= C_IDLE;
			end

			default: begin
				state <= C_IDLE;
			end
			endcase
		end
	end


///////////////////////////////////////////////////////
// sdram controller                                  //
///////////////////////////////////////////////////////

	wire cmd_ack;
	wire data_valid;
	wire[15:0] cmd_dout;
	wire[15:0] sd_dout;
	wire sd_oe;

	// for K4S561632(4Mx16x4)
	tsdram #(.ROW_BITS(13), .COL_BITS(9)) _tsd(
		reset, clk,
		ext_refresh, cmd_req, cmd_ack, cmd_mask, cmd_addr, cmd_din, cmd_dout, data_valid,
		sd_cke, sd_cs, sd_ras, sd_cas, sd_we, sd_addr, sd_ba, sd_dqm, sd_data, sd_dout, sd_oe
	);

	assign sd_data = (sd_oe)? sd_dout : 16'hzzzz;


endmodule

