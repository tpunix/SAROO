
///////////////////////////////////////////////////////
// Module: external bus master for STM32 FSMC        //
///////////////////////////////////////////////////////

module ext_master(
	addr, ncs, rd_start, wr_start, byte_en, data_in, data_out, wait_out,
	avm_clk, avm_reset, avm_addr, avm_rd, avm_rdvalid, avm_rdata, avm_wr, avm_wdata, avm_byte_en, avm_wait
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// external signal
	input[31:0] addr;
	input ncs;
	input rd_start;
	input wr_start;
	input[1:0] byte_en;
	input[15:0] data_in;
	output[15:0] data_out;
	output wait_out;

	// avalon master
	input avm_clk;
	input avm_reset;
	output[31:0] avm_addr;
	output avm_rd;
	input[15:0] avm_rdata;
	input avm_rdvalid;
	output avm_wr;
	output[15:0] avm_wdata;
	output[1:0] avm_byte_en;
	input avm_wait;

///////////////////////////////////////////////////////
//  master state                                     //
///////////////////////////////////////////////////////

	reg[2:0] mstate;
	reg avm_rd;
	reg avm_wr;
	reg emm_wait;
	reg cs_wait;
	reg[1:0] avm_byte_en;
	reg[15:0] avm_wdata;
	reg[31:0] avm_addr;
	reg[15:0] data_out;

	localparam M_IDLE=0, M_READ=1, M_READ_DATA=2, M_READ_END=3, M_WRITE=4, M_WRITE_END=5;

	always @(posedge avm_reset or posedge avm_clk)
	begin
		if(avm_reset==1) begin
			mstate <= M_IDLE;
		end else begin
			case(mstate)
			M_IDLE: begin
				if(ncs==0 && rd_start==1) begin
					mstate <= M_READ;
					avm_rd <= 1;
					emm_wait <= 1;
				end else if(ncs==0 && wr_start==1) begin
					mstate <= M_WRITE;
					avm_wr <= 1;
					emm_wait <= 1;
				end else begin
					avm_rd <= 0;
					avm_wr <= 0;
					emm_wait <= 0;
					mstate <= M_IDLE;
				end
			end

			M_READ: begin
				if(avm_wait==0) begin
					mstate <= M_READ_DATA;
					avm_rd <= 0;
				end
			end
			M_READ_DATA: begin
				if(avm_rdvalid==1) begin
					emm_wait <= 0;
					mstate <= M_IDLE;
				end
			end

			M_WRITE: begin
				if(avm_wait==0) begin
					emm_wait <= 0;
					avm_wr <= 0;
					mstate <= M_IDLE;
				end
			end

			default: begin
				mstate <= M_IDLE;
			end
			endcase
		end
	end


	always @(posedge emm_wait or negedge ncs)
	begin
		if(emm_wait==1)
			cs_wait <= 0;
		else
			cs_wait <= 1;
	end

	always @(posedge avm_clk)
	begin
		if(avm_rdvalid==1)
			data_out <= avm_rdata;
	end

	always @(posedge avm_clk)
	begin
		if(ncs==0 && wr_start==1) begin
			avm_wdata <= data_in;
			avm_byte_en <= byte_en;
		end
	end

	always @(posedge avm_clk)
	begin
		if(ncs==0 && (wr_start==1 || rd_start==1))
			avm_addr <= addr;
	end

	assign wait_out = ~(cs_wait | emm_wait);

endmodule

