
///////////////////////////////////////////////////////
// Module: Cached Bus Controller                     //
///////////////////////////////////////////////////////

module cachebus(
	reset, clk,
	cs, rd, wr, nwait, addr, wdata, rdata,
	cmd_req, cmd_ack, cache_invalid, cache_update,
	cache_addr, cache_data_1d, cache_valid
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


	localparam B_IDLE=0, B_WAITDATA=1, B_READ=2, B_WRITE=3;
	localparam CMD_IDLE=2'd0, CMD_WRITE=2'd1, CMD_READ=2'd2;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			state <= B_IDLE;
			nwait <= 1;
			cmd_req <= 0;
			cache_invalid <= 0;
			cache_update <= 0;
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
						state <= B_READ;
					end
				end else if(wr && cs) begin
					// 发出写请求. 如果命中Cache, 则更新数据.
					nwait <= 0;
					cmd_req <= CMD_WRITE;
					cache_update <= 1;
					state <= B_WRITE;
				end else begin
					nwait <= 1;
					cmd_req <= 0;
					cache_invalid <= 0;
					cache_update <= 0;
				end
			end

			B_READ: begin
				cache_invalid <= 0;
				if(cmd_ack) begin
					cmd_req <= 0;
					state <= B_WAITDATA;
				end
			end

			B_WAITDATA: begin
				if(cache_valid[addr[2:1]]) begin
					nwait <= 1;
					rdata <= cache_data[addr[2:1]];
					state <= B_IDLE;
				end
			end

			B_WRITE: begin
				cache_update <= 0;
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

