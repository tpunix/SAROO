
///////////////////////////////////////////////////////
// Module: SEGA Saturn CDC Data FIFO                 //
///////////////////////////////////////////////////////


module cdc_fifo(
	reg_fifo_ctrl, reg_blk_addr, reg_blk_size, reg_fifo_stat,
	rd_start, data_out, blk_dma_end,
	avm_clk, avm_reset, avm_addr, avm_rd, avm_rdvalid, avm_rdata, avm_wait
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	input[15:0] reg_fifo_ctrl;
	input[31:0] reg_blk_addr;
	input[15:0] reg_blk_size;
	output[15:0] reg_fifo_stat;

	input rd_start;
	output[15:0] data_out;
	output blk_dma_end;

	// avalon master
	input avm_clk;
	input avm_reset;
	output[31:0] avm_addr;
	output avm_rd;
	input[15:0] avm_rdata;
	input avm_rdvalid;
	input avm_wait;


///////////////////////////////////////////////////////
// FIFO                                              //
///////////////////////////////////////////////////////

	wire[10:0] usedw;
	wire full;
	wire empty;

	wire fifo_reset = reg_fifo_ctrl[2];

	fifo4k f4k(
		.aclr  (fifo_reset),
		.clock (avm_clk),

		.rdreq (rd_start),
		.q     (data_out),

		.wrreq (avm_rdvalid),
		.data  (avm_rdata),

		.empty (empty),
		.full  (full),
		.usedw (usedw)
	);

	wire[15:0] reg_fifo_stat = {mstate, empty, full, usedw};

///////////////////////////////////////////////////////
//  master state                                     //
///////////////////////////////////////////////////////

	reg[2:0] mstate;
	reg[15:0] dma_count;
	reg[31:0] avm_addr;
	reg avm_rd;
	reg blk_dma_end;

	wire dma_start = reg_fifo_ctrl[0];
	wire dma_abort = reg_fifo_ctrl[1];

	localparam M_IDLE=3'd0, M_LOAD=3'd1, M_READ_START=3'd2, M_READ_WAIT=3'd3, M_READ_NEXT=3'd4, M_READ_END=3'd5;

	always @(posedge avm_reset or posedge avm_clk)
	begin
		if(avm_reset==1) begin
			mstate <= M_IDLE;
		end else begin
			case(mstate)
			M_IDLE: begin
				if(dma_abort==0 && dma_start==1) begin
					mstate <= M_LOAD;
				end else begin
					blk_dma_end <= 1'b0;
					avm_rd <= 0;
				end
			end

			M_LOAD: begin
				avm_addr  <= reg_blk_addr;
				dma_count <= reg_blk_size;
				if(dma_abort==1)
					mstate <= M_IDLE;
				else if(usedw[10:9]==2'b00)
					mstate = M_READ_START;
			end

				M_READ_START: begin
					avm_rd <= 1;
					mstate <= M_READ_WAIT;
				end
				M_READ_WAIT: begin
					if(avm_wait==0) begin
						mstate <= M_READ_NEXT;
						avm_rd <= 0;
						dma_count <= dma_count-16'd2;
					end
				end
				M_READ_NEXT: begin
					avm_addr  <= avm_addr+32'd2;
					if(dma_abort==1) begin
						mstate <= M_IDLE;
					end else if(dma_count==0) begin
						mstate <= M_READ_END;
					end else begin
						mstate <= M_READ_START;
					end
				end

			M_READ_END: begin
				if(dma_start==0) begin
					blk_dma_end <= 1;
					mstate <= M_IDLE;
				end
			end

			default: begin
				mstate <= M_IDLE;
			end
			endcase
		end
	end

endmodule

