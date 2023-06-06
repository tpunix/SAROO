
///////////////////////////////////////////////////////
// Module: Tiny SDRAM Controller                     //
///////////////////////////////////////////////////////

module tsdram(
	reset,
	clk,

	ext_refresh,
	cmd_req,
	cmd_ack,
	cmd_mask,
	cmd_addr,
	cmd_din,
	cmd_dout,
	data_valid,

	sd_cke,
	sd_cs,
	sd_ras,
	sd_cas,
	sd_we,
	sd_addr,
	sd_ba,
	sd_dqm,
	sd_din,
	sd_dout,
	sd_oe
);

// cmd_req:
//   00: nop
//   01: write byte
//   11: write word
//   10: read  word

	input reset;
	input clk;

	input ext_refresh;
	input [ 1:0] cmd_req;
	output cmd_ack;
	input [ 1:0] cmd_mask;
	input [31:0] cmd_addr;
	input [15:0] cmd_din;
	output[15:0] cmd_dout;
	output data_valid;

	output sd_cke;
	output sd_cs;
	output sd_ras;
	output sd_cas;
	output sd_we;
	output[12:0] sd_addr;
	output[ 1:0] sd_ba;
	output[ 1:0] sd_dqm;
	input [15:0] sd_din;
	output[15:0] sd_dout;
	output sd_oe;

///////////////////////////////////////////////////////
// Module Parameters                                 //
///////////////////////////////////////////////////////

	// default: MT48LC32M16A2-75 512Mb
	// bank(4) x row(8192) x col(1024) 
	parameter tRC  = 7;
	parameter tRAS = 5;
	parameter tRCD = 2;
	parameter tCL  = 2;
	parameter tWR  = 2;
	parameter tRP  = 2;
	parameter tRFC = 7;
	parameter tCK  = 10;

	parameter BL = 4;
	parameter COL_BITS = 10;
	parameter ROW_BITS = 13;

	localparam tPOR_count = (200000/tCK) + 100;
	localparam tREF_count = (64000000/(1<<ROW_BITS))/tCK;


///////////////////////////////////////////////////////
// Address Map                                       //
///////////////////////////////////////////////////////

	// A31 - A26 [A25 - A13] [A12 A11] [A10 - A1] A0
	//           [R12 -  R0] [ B1  B0] [ C9 - C0] 

	localparam BANK_POS = COL_BITS+1;
	localparam ROW_POS  = BANK_POS+2;

	wire[ROW_BITS-1:0] col_addr = {3'b001, {(10-COL_BITS){1'b0}}, cmd_addr[COL_BITS:1]};

	wire[1:0] bank_addr = cmd_addr[COL_BITS+2:COL_BITS+1];
	wire[ROW_BITS-1:0] row_addr = cmd_addr[ROW_BITS+COL_BITS+2:COL_BITS+3];


///////////////////////////////////////////////////////
// Refresh Counter                                   //
///////////////////////////////////////////////////////

	reg[15:0] ref_count;
	reg ref_req;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			ref_count <= tPOR_count[15:0];
		end else if(ref_ack) begin
			ref_count <= tREF_count[15:0];
		end else if(ref_count) begin
			ref_count <= ref_count-1'b1;
		end
	end


	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			ref_req <= 0;
		end else if(ref_count==1 || ext_refresh) begin
			ref_req <= 1;
		end else if(ref_ack) begin
			ref_req <= 0;
		end
	end


///////////////////////////////////////////////////////
// Main state                                        //
///////////////////////////////////////////////////////

	reg[ 3:0] m_cmd;
	reg[ 1:0] m_dqm;
	reg[ 1:0] m_ba;
	reg[12:0] m_addr;
	reg[15:0] m_dout;
	reg[ 3:0] m_state;
	reg[ 3:0] m_next;
	reg[ 3:0] m_count;
	reg m_cke;
	reg m_oe;
	reg ref_ack;
	reg cmd_ack;

	localparam  M_POR=0,
				M_IDLE=1,
				M_WAIT=2,
				M_ACTIVE=3,
				M_REFERSH=4,
				M_READ=5,
				M_READ_END=6,
				M_WRITE=7,
				M_WRITE_WAIT=8;


	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			m_state <= M_POR;
			m_cmd <= 4'b1111;
			m_dqm <= 2'b11;
			m_ba <= 0;
			m_addr <= 13'h1fff;
			m_oe <= 0;
			m_cke <= 0;
		end else begin
			case(m_state)
			M_POR: begin
				if(ref_count==500) begin
					m_cke <= 1;
				end else if(ref_count==40) begin
					m_cmd <= 4'b0010;
				end else if(ref_count==30) begin
					m_cmd <= 4'b0001;
				end else if(ref_count==20) begin
					m_cmd <= 4'b0001;
				end else if(ref_count==10) begin
					m_cmd <= 4'b0000;
					// CL=2 RBL=4 WBL=1
					m_addr <= {3'b000,1'b1,2'b00,3'b010,4'b0010};
				end else if(ref_count==0) begin
					m_state <= M_IDLE;
				end else begin
					m_cmd <= 4'b0111;
				end
			end

			M_IDLE: begin
				m_dqm <= 2'b11;
				m_oe <= 0;
				if(ref_req) begin
					ref_ack <= 1'b1;
					m_state <= M_REFERSH;
				end else if(cmd_req==2'b10) begin
					m_state <= M_ACTIVE;
					m_next <= M_READ;
				end else if(cmd_req==2'b01 || cmd_req==2'b11) begin
					m_state <= M_ACTIVE;
					m_next <= M_WRITE;
				end else begin
					m_cmd <= 4'b1111;
				end
			end

			M_WAIT: begin
				m_cmd <= 4'b0111;
				if(m_count)
					m_count <= m_count-1'b1;
				else
					m_state <= m_next;
			end

			M_ACTIVE: begin
				m_cmd <= 4'b0011;
				m_addr <= row_addr;
				m_ba <= bank_addr;
				m_count <= tRCD[3:0]-4'd2;
				m_state <= M_WAIT;
			end

			M_REFERSH: begin
				ref_ack <= 1'b0;
				m_cmd <= 4'b0001;
				m_count <= tRFC[3:0];
				m_state <= M_WAIT;
				m_next <= M_IDLE;
			end

			M_READ: begin
				m_cmd <= 4'b0101;
				m_dqm <= 2'b00;
				m_addr <= col_addr;
				m_count <= BL[3:0]+tRP[3:0]-4'd1;
				m_state <= M_WAIT;
				m_next <= M_IDLE;
			end

			M_WRITE: begin
				m_cmd <= 4'b0100;
				m_dqm <= cmd_mask;
				m_addr <= col_addr;
				m_dout <= cmd_din;
				m_oe <= 1;
				m_count <= tWR[3:0]+tRP[3:0]-4'd1;
				m_state <= M_WAIT;
				m_next <= M_IDLE;
			end

			default: begin
				m_state <= M_IDLE;
			end
			endcase

		end
	end


	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cmd_ack <= 0;
		end else if(m_state==M_READ || m_state==M_WRITE) begin
			cmd_ack <= 1;
		end else begin
			cmd_ack <= 0;
		end
	end


	reg[2:0] cl_count;
	reg data_valid;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cl_count <= 0;
		end else begin
			if(m_state==M_READ) begin
				cl_count <= 1;
			end else if(cl_count==tCL+BL) begin
				cl_count <= 0;
			end else if(cl_count) begin
				cl_count <= cl_count+1'b1;
			end
		end
	end

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			data_valid <= 0;
		end else if (cl_count==tCL) begin
			data_valid <= 1;
		end else if (cl_count==tCL+BL) begin
			data_valid <= 0;
		end
	end

//	reg[15:0] cmd_dout;
//	always @(posedge clk)
//	begin
//		cmd_dout <= sd_din;
//	end
	wire[15:0] cmd_dout = sd_din;


///////////////////////////////////////////////////////
// SDRAM signals                                     //
///////////////////////////////////////////////////////

	assign sd_cke = m_cke;
	assign {sd_cs, sd_ras, sd_cas, sd_we} = m_cmd;
	assign sd_addr = m_addr;
	assign sd_ba = m_ba;
	assign sd_dqm = m_dqm;
	assign sd_dout = m_dout;
	assign sd_oe = m_oe;

endmodule

