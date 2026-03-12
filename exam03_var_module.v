`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/09/2025 09:39:20 AM
// Design Name: 
// Module Name: exam03_var_module
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module watch(
    input clk, reset_p,
    input [2:0] btn,
    output reg [7:0] sec, min);
    
    reg set_watch;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)set_watch = 0;
        else if(btn[0])set_watch = ~set_watch;
    end
    
    integer cnt_sysclk;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            cnt_sysclk = 0;
            sec = 0;
            min = 0;
        end
        else begin
            if(set_watch)begin
                if(btn[1])begin
                    if(sec >= 59)sec = 0;
                    else sec = sec + 1;
                end
                if(btn[2]) begin
                    if(min >= 59)min = 0;
                    else min = min + 1;
                end
            end
            if(cnt_sysclk == 27'd99_999_999)begin
                cnt_sysclk = 0;
                if(sec >= 59)begin
                    sec = 0;
                    if(min >= 59)min = 0;
                    else min = min + 1;                    
                end
                else sec = sec + 1;
            end
            else cnt_sysclk = cnt_sysclk + 1;        
        end
    end    
endmodule

module cook_timer(
    input clk, reset_p,
    input btn_start, inc_sec, inc_min, alarm_off,
    output reg [7:0] sec, min,
    output reg alarm);
    
    reg [7:0]set_sec, set_min;
    reg dcnt_set;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            dcnt_set = 0;
            alarm = 0;
            set_sec = 0;
            set_min = 0;
            
        end
        else begin
            if(btn_start)begin
                dcnt_set = ~dcnt_set;
                set_sec = sec;
                set_min = min;
            end
            if(sec == 0 && min == 0 && dcnt_set)begin
                dcnt_set = 0;
                alarm = 1;
            end
            if(alarm_off || inc_sec || inc_min)alarm = 0;            
        end
    end
    integer cnt_sysclk;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            cnt_sysclk = 0;
            sec = 0;
            min = 0;
        end
        else begin
            if(dcnt_set)begin
                if(cnt_sysclk >= 99_999_999)begin
                    cnt_sysclk = 0;
                    if(sec == 0 && min)begin
                        sec = 59;
                        min = min -1;
                    end
                    else sec = sec -1;
                end
                else cnt_sysclk = cnt_sysclk + 1;
            end
            else if(alarm_off)begin
                sec = set_sec;
                min = set_min;
            end
            else begin
                if(inc_sec)begin
                    if(sec >= 59)begin
                        sec = 0;
                    end
                    else begin
                        sec = sec + 1;
                    end
                end
                if(inc_min)begin
                    if(min >= 59)begin
                        min = 0;
                    end
                    else begin
                        min = min + 1;
                    end
                end
            end
        end
    end

endmodule

module stop_watch(
    input clk, reset_p,
    input btn_start, lab_rec,
    output reg [7:0] csec, sec, lab_csec, lab_sec,
    output reg lab_pick);
    

    reg dcnt_set;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            lab_pick = 0;
            lab_csec = 0;
            lab_sec = 0;
            
        end
        else begin
            if(btn_start)begin
                dcnt_set = ~dcnt_set;
            end
            else if(dcnt_set && lab_rec)begin
                lab_csec = csec;
                lab_sec = sec;
                lab_pick = ~lab_pick;
            end
        end
    end
    integer cnt_sysclk;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            cnt_sysclk = 0;
            csec = 0;
            sec = 0;
        end
        else begin
            if(dcnt_set)begin
                if(cnt_sysclk >= 999_999)begin
                    cnt_sysclk = 0;
                    if(csec >= 99)begin
                        csec = 0;
                        sec = sec +1;
                        if(sec >= 60) sec = 0; 
                    end
                    else csec = csec +1;
                end
                else cnt_sysclk = cnt_sysclk + 1;
            end
            
        end
    end

endmodule

module stop_watch2(
    input clk, reset_p,
    input btn_start, btn_lap, btn_clear,
    output reg [7:0] fnd_sec, fnd_csec,
    output reg start_stop, lap);
    
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            start_stop = 0;
        end
        else begin
            if(btn_start)start_stop = ~start_stop;
            else if(btn_clear)start_stop = 0;
        end
    end
    
    reg [7:0] sec, csec, lap_sec, lap_csec;
    integer cnt_sysclk;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            cnt_sysclk = 0;
            sec = 0;
            csec = 0;
        end
        else begin
            if(start_stop)begin
                if(cnt_sysclk >= 999_999)begin
                    cnt_sysclk = 0;
                    if(csec >= 99)begin
                        csec = 0;
                        if(sec >= 59)begin
                            sec = 0;
                        end
                        else sec = sec + 1;
                    end
                    else csec = csec + 1;
                end
                else cnt_sysclk = cnt_sysclk + 1;
            end
            if(btn_clear)begin
                sec = 0;
                csec = 0;
                cnt_sysclk = 0;
            end
        end
    end
    
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            lap_sec = 0;
            lap_csec = 0;
            lap = 0;
        end
        else begin
            if(btn_lap)begin
                if(start_stop)lap = ~lap;
                lap_sec = sec;
                lap_csec = csec;
            end
            if(btn_clear)begin
                lap = 0;
                lap_sec = 0;
                lap_csec = 0;
            end
        end
    end
    
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            fnd_sec = 0;
            fnd_csec = 0;
        end
        else begin
            if(lap)begin
                fnd_sec = lap_sec;
                fnd_csec = lap_csec;
            end
            else begin
                fnd_sec = sec;
                fnd_csec = csec;
            end
        end
    end
    

endmodule

module clock_time_core(
    input  wire clk,
    input  wire reset_p,
    input  wire btn_edit,   // 버튼1
    input  wire btn_field,  // 버튼2
    input  wire btn_inc,    // 버튼3
    output reg  [7:0] hh,
    output reg  [7:0] mm,
    output reg  [7:0] ss,
    output reg  edit_mode,   // 1=수정모드
    output reg  [1:0]field_sel    // 0=HH, 1=MM, 2=SS
);
    
    
    reg btn_ed_pedge, btn_field_pedge, btn_inc_pedge;
    edge_detector_p edit(.clk(clk), .reset_p(reset_p), .cp(btn_edit), .p_edge(btn_ed_pedge));
    edge_detector_p field(.clk(clk), .reset_p(reset_p), .cp(btn_field), .p_edge(btn_field_pedge));
    edge_detector_p inc(.clk(clk), .reset_p(reset_p), .cp(btn_inc), .p_edge(btn_inc_pedge));
    // 1초 분주(100MHz 가정)
    reg [26:0] sec_div;
    wire tick_1s = (sec_div == 27'd99_999_999);

    always @(posedge clk or posedge reset_p) begin
        if(reset_p) begin
            sec_div <= 0;
        end else begin
            if(tick_1s) sec_div <= 0;
            else        sec_div <= sec_div + 1;
        end
    end

    // edit/field 제어
    always @(posedge clk or posedge reset_p) begin
        if(reset_p) begin
            edit_mode <= 1'b0;
            field_sel <= 2'd0;
        end else begin
            if(btn_ed_pedge) edit_mode <= ~edit_mode;
            if(btn_field_pedge) begin
                if(field_sel == 2'd2) field_sel <= 2'd0;
                else                  field_sel <= field_sel + 1;
            end
        end
    end

    // 시간 증가/감소 유틸 (범위 wrap)
    task inc_wrap(input [1:0] sel);
    begin
        case(sel)
            2'd0: hh <= (hh >= 8'd23) ? 8'd0  : (hh + 1);
            2'd1: mm <= (mm >= 8'd59) ? 8'd0  : (mm + 1);
            2'd2: ss <= (ss >= 8'd59) ? 8'd0  : (ss + 1);
        endcase
    end
    endtask

    task dec_wrap(input [1:0] sel);
    begin
        case(sel)
            2'd0: hh <= (hh == 0) ? 8'd23 : (hh - 1);
            2'd1: mm <= (mm == 0) ? 8'd59 : (mm - 1);
            2'd2: ss <= (ss == 0) ? 8'd59 : (ss - 1);
        endcase
    end
    endtask

    // 실제 hh:mm:ss 동작
    always @(posedge clk or posedge reset_p) begin
        if(reset_p) begin
            hh <= 0; mm <= 0; ss <= 0;
        end else begin
            // 수정모드 ON: 버튼으로만 증감
            if(edit_mode) begin
                if(btn_inc_pedge) inc_wrap(field_sel);
            end
            // 수정모드 OFF: 1초마다 정상 시계
            else begin
                if(tick_1s) begin
                    if(ss == 8'd59) begin
                        ss <= 0;
                        if(mm == 8'd59) begin
                            mm <= 0;
                            hh <= (hh == 8'd23) ? 0 : (hh + 1);
                        end else begin
                            mm <= mm + 1;
                        end
                    end else begin
                        ss <= ss + 1;
                    end
                end
            end
        end
    end
endmodule
