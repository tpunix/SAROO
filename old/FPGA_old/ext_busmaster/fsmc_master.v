
///////////////////////////////////////////////////////
// Module: external bus master for STM32 FSMC        //
///////////////////////////////////////////////////////

module fsmc_master(
	addr, ale, ncs, nrd, nwr, data_in, data_out, wait_out,
	avm_clk, avm_reset, avm_addr, avm_rd, avm_rdvalid, avm_rdata, avm_wr, avm_wdata, avm_wait
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// external signal
	input[31:0] addr;
	input ale;
	input ncs;
	input nrd;
	input nwr;
	input[15:0] data_in;
	output[15:0] data_out;
	output wait_out;

	// io master
	input avm_clk;
	input avm_reset;
	output[31:0] avm_addr;
	output avm_rd;
	input[15:0] avm_rdata;
	input avm_rdvalid;
	output avm_wr;
	output[15:0] avm_wdata;
	input avm_wait;

///////////////////////////////////////////////////////
//  input sync                                       //
///////////////////////////////////////////////////////

	reg ale_s0, ale_s1, ale_s2;

	always @(posedge avm_clk)
	begin
		ale_s0 <= ale;
		ale_s1 <= ale_s0;
		ale_s2 <= ale_s1;
	end
	wire rd_start = (ale_s2==0 && ale_s1==1 && ncs==0 && nrd==0);

	reg nwr_s0, nwr_s1, nwr_s2;

	always @(posedge avm_clk)
	begin
		nwr_s0 <= nwr;
		nwr_s1 <= nwr_s0;
		nwr_s2 <= nwr_s1;
	end
	wire wr_start = (nwr_s2==1 && nwr_s1==0 && ncs==0);

///////////////////////////////////////////////////////
//  master read                                      //
///////////////////////////////////////////////////////

	reg[2:0] mstate;
	reg avm_rd;
	reg avm_wr;
	reg emm_wait;
	reg cs_wait;
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
				if(rd_start==1) begin
					mstate <= M_READ;
					avm_rd <= 1;
					emm_wait <= 1;
				end else if(wr_start==1) begin
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
		if(wr_start==1)
			avm_wdata <= data_in;
	end

	always @(posedge avm_clk)
	begin
		if(wr_start==1 || rd_start==1)
			avm_addr <= addr;
	end

	assign wait_out = ~(cs_wait | emm_wait);

endmodule

