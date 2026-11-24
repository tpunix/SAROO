
///////////////////////////////////////////////////////
// Module: Cached Bus Controller                     //
///////////////////////////////////////////////////////

module cachebus(
	reset, clk,
	cs, rd, wr, nwait, addr, wdata, rdata,
	cmd_req, cmd_ack, cache_invalid, cache_update,
	cache_addr, cache_data_1d, cache_valid,
	mask, req_addr, req_wdata, req_mask, hit,
	fill_valid, fill_data
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// system
	input reset;
	input clk;

	// bus input
	input cs;
	input rd;
	input wr;
	output nwait;
	input[25:0] addr;
	input[15:0] wdata;
	output[15:0] rdata;

	// request output
	output[1:0] cmd_req;
	input cmd_ack;
	output cache_invalid;
	output cache_update;

	// cache interface
	input[25:3] cache_addr;
	input[63:0] cache_data_1d;
	input[ 3:0] cache_valid;

	// [ABUS_EXP] Change 3: request captured when it is seen
	input [ 1:0] mask;
	output[25:0] req_addr;
	output[15:0] req_wdata;
	output[ 1:0] req_mask;
	output hit;		// [ABUS_EXP] Change 4: one-clock pulse, read served from the line cache
	input fill_valid;	// [ABUS_EXP] Change 6a: a word of this port's line fill is on fill_data now
	input[15:0] fill_data;


///////////////////////////////////////////////////////
// bus controller                                    //
///////////////////////////////////////////////////////

	wire[15:0] cache_data[3:0];
	assign cache_data[0] = cache_data_1d[15: 0];
	assign cache_data[1] = cache_data_1d[31:16];
	assign cache_data[2] = cache_data_1d[47:32];
	assign cache_data[3] = cache_data_1d[63:48];


	reg[ 1:0] cmd_req;
	reg cache_invalid;
	reg cache_update;
	reg[15:0] rdata;
	reg[1:0] state;
	reg nwait;
	reg[25:0] req_addr;
	reg[15:0] req_wdata;
	reg[ 1:0] req_mask;
	reg first_fill;		// [ABUS_EXP] Change 6a: the next fill word is the requested one


	localparam B_IDLE=0, B_WAITDATA=1, B_READ=2, B_WRITE=3;
	localparam CMD_IDLE=2'd0, CMD_WRITE=2'd1, CMD_READ=2'd2;

	// [ABUS_EXP] Change 3: latch the bus on the clock the request is seen
	// (same conditions that start a read miss or a write in B_IDLE below).
	reg hit;
	always @(posedge clk)
		hit <= (state==B_IDLE && rd && cs && addr[25:3]==cache_addr);

	// [ABUS_EXP] Change 7: after a write is seen, keep sampling the write data and mask for WTRACK_CLKS
	// more clocks (it's still on the bus: Sega's write cycle is ~140ns). On hardware the data latched at
	// wr_start (~40ns after CS0) could still be settling (a bit kept the previous value).
	localparam WTRACK_CLKS = 3'd4;
	reg[2:0] wtrack;

	always @(posedge clk)
	begin
		if(state==B_IDLE && cs && ((rd && addr[25:3]!=cache_addr) || (!rd && wr))) begin
			req_addr  <= addr;
			req_wdata <= wdata;
			req_mask  <= mask;
			wtrack    <= (!rd && wr)? WTRACK_CLKS : 3'd0;
		end else if(wtrack) begin
			req_wdata <= wdata;
			req_mask  <= mask;
			wtrack    <= wtrack-1'b1;
		end
	end

	// [ABUS_EXP] Change 7: the cache copy is updated when the write is done, with the final data.
	// Was: a pulse on the clock the write was seen (with the first, possibly unsettled data).
	always @(negedge reset or posedge clk)
	begin
		if(reset==0)
			cache_update <= 0;
		else
			cache_update <= (state==B_WRITE && cmd_ack);
	end

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			state <= B_IDLE;
			nwait <= 1;
			cmd_req <= 0;
			cache_invalid <= 0;
		end else begin
			case(state)
			B_IDLE: begin
				if(rd && cs) begin
					if(addr[25:3]==cache_addr) begin
						// Cache命中.
						rdata <= cache_data[addr[2:1]];
					end else begin
						// Cache未命中, 发出读请求.
						nwait <= 0;
						cmd_req <= CMD_READ;
						cache_invalid <= 1;
						first_fill <= 1;
						state <= B_READ;
					end
				end else if(wr && cs) begin
					// 发出写请求. 如果命中Cache, 则更新数据.
					nwait <= 0;
					cmd_req <= CMD_WRITE;
					state <= B_WRITE;
				end else begin
					nwait <= 1;
					cmd_req <= 0;
					cache_invalid <= 0;
				end
			end

			B_READ: begin
				cache_invalid <= 0;
				if(fill_valid) first_fill <= 0;	// data came before WAITDATA: the cache path below picks it up
				if(cmd_ack) begin
					cmd_req <= 0;
					state <= B_WAITDATA;
				end
			end

			B_WAITDATA: begin
				// [ABUS_EXP] Change 6a: the burst starts at the requested word (Change 2), so the first fill
				// word is the one the Saturn is waiting for: take it straight from the SDRAM, one clock
				// earlier than through the cache register.
				if(fill_valid && first_fill) begin
					first_fill <= 0;
					nwait <= 1;
					rdata <= fill_data;
					state <= B_IDLE;
				end else if(cache_valid[req_addr[2:1]]) begin	// [ABUS_EXP] was addr[2:1] (live bus)
					first_fill <= 0;
					nwait <= 1;
					rdata <= cache_data[req_addr[2:1]];
					state <= B_IDLE;
				end
			end

			B_WRITE: begin
				if(cmd_ack) begin
					nwait <= 1;
					cmd_req <= 0;
					state <= B_IDLE;
				end
			end

			default: begin
				state <= B_IDLE;
			end
			endcase

		end
	end

endmodule

