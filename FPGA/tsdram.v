
///////////////////////////////////////////////////////
// Module: Tiny SDRAM Controller                     //
///////////////////////////////////////////////////////

module tsdram(
	reset,
	clk,
	reg_sdram,

	ext_refresh,
	cmd_req,
	cmd_prio,
	sat_cs,
	sat_hit,
	stm_ok,
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
	input [11:0] reg_sdram;

	input ext_refresh;
	input [ 1:0] cmd_req;
	input cmd_prio;		// [ABUS_EXP] 1: request is from the Saturn (served before refresh)
	input sat_cs;		// [ABUS_EXP] Saturn CS0 active (raw, asynchronous)
	input sat_hit;		// [ABUS_EXP] Saturn read served from the line cache (1-clock pulse)
	output stm_ok;		// [ABUS_EXP] Change 5: memhub may start an STM32 access now
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
	//           [R12 -  R0] [ B1  B0] [ C9 - C0]      64MB
	//
	// A31 - A25 [A24 - A12] [A11 A10] [ A9 - A1] A0
	//           [R12 -  R0] [ B1  B0] [ C8 - C0]      32MB
	//
	// A31 - A24 [A23 - A12] [A11 A10] [ A9 - A1] A0
	//           [R11 -  R0] [ B1  B0] [ C8 - C0]      16MB
	//
	// A31 - A23 [A22 - A11] [A10  A9] [ A8 - A1] A0
	//           [R11 -  R0] [ B1  B0] [ C7 - C0]       8MB

	localparam BANK_POS = COL_BITS+1;
	localparam ROW_POS  = BANK_POS+2;

//	wire[ROW_BITS-1:0] col_addr = {3'b001, {(10-COL_BITS){1'b0}}, cmd_addr[COL_BITS:1]};
//	wire[1:0] bank_addr = cmd_addr[COL_BITS+2:COL_BITS+1];
//	wire[ROW_BITS-1:0] row_addr = cmd_addr[ROW_BITS+COL_BITS+2:COL_BITS+3];

	wire[1:0] sdtype = reg_sdram[11:10];
	wire[12:0] col_addr =
		(sdtype==2'b11)? {3'b001,   cmd_addr[10:1]} :
		(sdtype==2'b10)? {4'b0010,  cmd_addr[ 9:1]} :
		(sdtype==2'b01)? {4'b0010,  cmd_addr[ 9:1]} :
							  {5'b00100, cmd_addr[ 8:1]} ;
	wire[1:0] bank_addr =
		(sdtype==2'b11)? cmd_addr[12:11] :
		(sdtype==2'b10)? cmd_addr[11:10] :
		(sdtype==2'b01)? cmd_addr[11:10] :
							  cmd_addr[10: 9] ;
	wire[12:0] row_addr =
		(sdtype==2'b11)? cmd_addr[25:13] :
		(sdtype==2'b10)? cmd_addr[24:12] :
		(sdtype==2'b01)? {1'b0, cmd_addr[23:12]} :
							  {1'b0, cmd_addr[22:11]} ;


///////////////////////////////////////////////////////
// Refresh Counter                                   //
///////////////////////////////////////////////////////

	reg[15:0] ref_count;

	// [SIM] declarations moved up for ModelSim (order only)
	reg ref_ack;

	// [ABUS_EXP] Changes 1 and 4: the refresh scheduler is below the main
	// state declarations (it needs m_state and M_POR).


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


///////////////////////////////////////////////////////
// [ABUS_EXP] Refresh scheduler (Changes 1 and 4)     //
///////////////////////////////////////////////////////
// Original: a refresh was requested every reg_sdram clocks (7.81us, timer
// restarted by each refresh) and on every Saturn refresh cycle (ext_refresh),
// and in M_IDLE it always went before any waiting request.
//
// Now:
// - The timer keeps running while a refresh is postponed and counts the owed
//   refreshes in ref_debt (the timer still restarts on each refresh, so the
//   average rate is unchanged: >= 1 per 7.81us).
// - Owed refreshes are issued only at moments when a Saturn access is
//   unlikely to arrive during the refresh:
//     after_sat : right after a Saturn READ has finished in the SDRAM (the
//                 Saturn's current cycle, its read-precharge clock and the
//                 next access's synchroniser still have to pass). Not after
//                 writes: no write precharge, the next access can follow;
//     in_hit    : a Saturn read is being answered from the line cache; the
//                 SDRAM is free for the rest of that bus cycle;
//     quiet     : CS0 inactive for QUIET_CLKS clocks;
//     ext_req   : a Saturn refresh cycle (FC1), when it is not accessing.
// - When REF_DEBT_MAX refreshes are owed, refresh is forced, even ahead of a
//   waiting Saturn request (worst case 4 x 7.81us between refreshes, well
//   inside the SDRAM's allowance of 8 postponed refreshes).
// - Otherwise a waiting Saturn request always goes first (Change 1).

	localparam SAT_WR_EXTRA = 4'd2;	// [ABUS_EXP] Change 7, see M_IDLE
	localparam REF_DEBT_MAX = 3'd4;
	localparam QUIET_CLKS   = 10'd64;	// 640ns

	wire in_por   = (m_state==M_POR);
	wire ref_tick = !in_por && (ref_count==1);

	reg[2:0] ref_debt;
	reg      ext_req;
	reg      cur_sat;		// the command being executed is the Saturn's
	reg      after_sat;		// just back in M_IDLE after a Saturn command
	reg[9:0] sat_quiet;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			ref_count <= tPOR_count[15:0];
		end else if(ref_ack || (!in_por && ref_count<=1)) begin
			ref_count <= {6'b0, reg_sdram[9:0]};
		end else if(ref_count) begin
			ref_count <= ref_count-1'b1;
		end
	end

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			ref_debt <= 0;
		end else if(ref_tick && !(ref_ack && ref_debt!=0)) begin
			if(ref_debt!=3'd7) ref_debt <= ref_debt+1'b1;
		end else if(!ref_tick && ref_ack && ref_debt!=0) begin
			ref_debt <= ref_debt-1'b1;
		end
	end

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			ext_req <= 0;
		end else if(ext_refresh) begin
			ext_req <= 1;
		end else if(ref_ack) begin
			ext_req <= 0;
		end
	end

	// Saturn bus activity, from CS0 itself (most Saturn reads hit the FPGA's
	// line cache and never reach the SDRAM, so SDRAM commands are not enough).
	// (A slot right after CS0 goes inactive was tried and rejected in
	// simulation: the next Saturn access can start ~35ns later and then
	// collides with the refresh.)
	reg[1:0] cs_sync;
	reg[1:0] hit_win;		// clocks left in the slot after a cache hit

	always @(posedge clk)
		cs_sync <= {cs_sync[0], sat_cs};

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			hit_win <= 0;
			sat_quiet <= 0;
		end else begin
			if(sat_hit)
				hit_win <= 2'd2;
			else if(hit_win)
				hit_win <= hit_win-1'b1;

			if(cs_sync[1])
				sat_quiet <= 0;
			else if(sat_quiet!=10'h3ff)
				sat_quiet <= sat_quiet+1'b1;
		end
	end

	wire in_hit      = (hit_win!=0);	// Saturn cycle being served from cache: SDRAM free until it ends
	wire ref_force   = (ref_debt>=REF_DEBT_MAX);
	wire quiet       = (sat_quiet>=QUIET_CLKS);
	wire ref_ok      = ext_req || (ref_debt!=0 && (after_sat || in_hit || quiet || ref_force));
	wire sat_waiting = (cmd_req!=2'b00) && cmd_prio;
	wire do_refresh  = ref_ok && !(sat_waiting && !ref_force);

///////////////////////////////////////////////////////
// [ABUS_EXP] Change 5: STM32 access slots            //
///////////////////////////////////////////////////////
// While the Saturn is using the cart with fast cycles (fast_bus, below) and
// CS0 was active within SAT_IDLE_CLKS, an STM32 access may only start in the
// same safe slots as owed refreshes:
//   - up to STM_HIT_WIN clocks after a Saturn cache hit (SDRAM free for the
//     rest of that bus cycle; one STM32 access, <=90ns, fits);
//   - the first idle clock after a Saturn read in the SDRAM (after_sat).
// A refresh that is owed or requested takes the slot first.
// When the Saturn has not used CS0 for SAT_IDLE_CLKS, the STM32 is free.
// memhub also lets a request through after it has waited STM_MAX_WAIT
// (memhub.v), so long runs of Saturn writes (no safe slot) can't starve it.
// The first Saturn access after an idle period can still meet an STM32
// access in progress.

	parameter  SAT_IDLE_CLKS = 10'd300;	// 3us (CS0 idle this long: STM32 free). Longer = fewer Saturn errors after pauses, less STM32 bandwidth
	localparam STM_HIT_WIN   = 3'd6;

	reg[2:0] stm_win;
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			stm_win <= 0;
		end else if(sat_hit) begin
			stm_win <= STM_HIT_WIN;
		end else if(stm_win) begin
			stm_win <= stm_win-1'b1;
		end
	end

	// Only needed when the Saturn runs fast cycles. With external WAIT on
	// (SAROO's 0x38803880 / 0x34403440) the Saturn waits for the SDRAM anyway,
	// so the STM32 is left alone. Fast mode = the last CS0 cycle lasted
	// <= FAST_CYCLE_CLKS (Sega 0x23301FF0: ~140ns = 14 clocks; 0x3440: >=175ns).
	localparam FAST_CYCLE_CLKS = 5'd16;
	reg[4:0] cs_len;
	reg      cs_last;
	reg      fast_bus;
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cs_len <= 0; cs_last <= 0; fast_bus <= 0;
		end else begin
			cs_last <= cs_sync[1];
			if(cs_sync[1]) begin
				if(cs_len!=5'h1f) cs_len <= cs_len+1'b1;
			end else begin
				if(cs_last) fast_bus <= (cs_len <= FAST_CYCLE_CLKS);
				cs_len <= 0;
			end
		end
	end

	wire sat_mode = fast_bus && (sat_quiet < SAT_IDLE_CLKS);
	wire stm_slot = (stm_win!=0) || (after_sat && m_state==M_IDLE);
	assign stm_ok = !sat_mode || (stm_slot && ref_debt==0 && !ext_req);


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
			cur_sat <= 0;
			after_sat <= 0;
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
				after_sat <= 0;
				if(do_refresh) begin	// [ABUS_EXP] was: if(ref_req)
					ref_ack <= 1'b1;
					cur_sat <= 0;
					m_state <= M_REFERSH;
				end else if(cmd_req==2'b10) begin
					cur_sat <= cmd_prio;
					// [ABUS_EXP] Change 6c: ACTIVE issued here instead of in M_ACTIVE one clock later
					// (ACT -> READ stays 2 clocks = tRCD through M_WAIT). Was: m_state <= M_ACTIVE;
					m_cmd <= 4'b0011;
					m_addr <= row_addr;
					m_ba <= bank_addr;
					m_count <= tRCD[3:0]-4'd2;
					m_state <= M_WAIT;
					m_next <= M_READ;
				end else if(cmd_req==2'b01 || cmd_req==2'b11) begin
					// no after_sat slot after a Saturn write: with Sega timing
					// (A0WPC=0) the next access can start right after it
					cur_sat <= 0;
					m_cmd <= 4'b0011;		// [ABUS_EXP] Change 6c, as for reads
					m_addr <= row_addr;
					m_ba <= bank_addr;
					// [ABUS_EXP] Change 7: a Saturn write waits SAT_WR_EXTRA more clocks, so the write data
					// cachebus keeps sampling (WTRACK_CLKS) is final when M_WRITE takes it
					m_count <= tRCD[3:0]-4'd2 + (cmd_prio? SAT_WR_EXTRA : 4'd0);
					m_state <= M_WAIT;
					m_next <= M_WRITE;
				end else begin
					m_cmd <= 4'b1111;
				end
			end

			M_WAIT: begin
				m_cmd <= 4'b0111;
				if(m_count)
					m_count <= m_count-1'b1;
				else begin
					m_state <= m_next;
					// [ABUS_EXP] Change 4: mark the first M_IDLE clock after a Saturn command
					if(m_next==M_IDLE) after_sat <= cur_sat;
				end
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

