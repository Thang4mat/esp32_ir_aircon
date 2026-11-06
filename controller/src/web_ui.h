#pragma once
#include <Arduino.h>

// Trang điều khiển máy lạnh — HTML tĩnh, nhúng thẳng trong firmware (PROGMEM).
// Gọi qua server.send_P() để không copy chuỗi lớn này ra heap mỗi lần có request.
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Điều khiển máy lạnh</title>
<style>
  :root {
    --bg:#15171A; --card:#1E2126; --card-border:#2A2E35;
    --text:#E8E8E6; --text-dim:#9A9DA3;
    --accent:#4F9CF0; --accent-text:#0B1420;
    --on:#3B8F4A; --off:#B5473A;
    --btn-bg:#262A31; --btn-text:#E8E8E6;
  }
  html[data-theme="light"] {
    --bg:#F1EFE8; --card:#FFFFFF; --card-border:#E2E0D8;
    --text:#2C2C2A; --text-dim:#6B6B68;
    --accent:#185FA5; --accent-text:#FFFFFF;
    --on:#3B6D11; --off:#993C1D;
    --btn-bg:#E6E4DB; --btn-text:#2C2C2A;
  }
  * { box-sizing: border-box; }
  body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;max-width:460px;margin:0 auto;
       padding:16px 16px 40px;background:var(--bg);color:var(--text);transition:background .15s,color .15s;}
  h1{font-size:19px;text-align:center;margin:8px 0 20px;flex:1;}
  .topbar{display:flex;align-items:center;justify-content:space-between;}
  .theme-btn{background:var(--btn-bg);color:var(--btn-text);border:none;border-radius:8px;
             padding:8px 12px;font-size:13px;cursor:pointer;}
  .card{background:var(--card);border:1px solid var(--card-border);border-radius:12px;
        padding:16px;margin-bottom:12px;}
  .card h2{font-size:13px;font-weight:600;margin:0 0 4px;color:var(--text);}
  .caption{font-size:12px;color:var(--text-dim);margin:0 0 10px;line-height:1.5;}
  .row{display:flex;flex-wrap:wrap;justify-content:center;gap:8px;}
  button{padding:10px 16px;border:none;border-radius:8px;background:var(--btn-bg);
         color:var(--btn-text);font-size:14px;cursor:pointer;}
  button:disabled{opacity:.4;}
  button.active{background:var(--accent);color:var(--accent-text);}
  button.power-on{background:var(--on);color:#fff;}
  button.power-off{background:var(--off);color:#fff;}
  #temp{font-size:32px;font-weight:600;text-align:center;margin:6px 0;}
  select,input[type=number],input[type=time]{background:var(--btn-bg);color:var(--btn-text);border:none;
         border-radius:8px;padding:8px 10px;font-size:14px;width:100%;}
  .lbl{display:block;font-size:12px;color:var(--text-dim);margin:10px 0 4px;}
  .atrow{display:flex;align-items:center;gap:10px;margin-top:10px;}
  .atrow label{flex:1 0 90px;font-size:14px;}
  .atrow input[type=time]{width:auto;flex:1;}
  .timer-actions{margin-top:14px;}
  .timer-status{margin-top:12px;font-size:13px;line-height:1.6;}
  .timer-status div{margin:2px 0;}
  .timer-status .dim{color:var(--text-dim);}
  .timer-status .warn{color:var(--off);}
  #status{font-size:12px;text-align:center;color:var(--text-dim);margin-top:8px;}
</style>
</head>
<body>
  <div class="topbar">
    <span style="width:70px"></span>
    <h1>Điều khiển máy lạnh Midea</h1>
    <button class="theme-btn" id="themeBtn" onclick="toggleTheme()">🌙</button>
  </div>

  <div class="card">
    <h2>Nguồn</h2>
    <p class="caption">Bật hoặc tắt máy lạnh. Bật/Tắt xóa mọi hẹn giờ đang đặt. Khi đang tắt, đổi nhiệt độ/chế độ/quạt chỉ được lưu và áp dụng khi bấm Bật.</p>
    <div class="row">
      <button id="btnOn" onclick="call('/api/power', {on: 1})">Bật</button>
      <button id="btnOff" onclick="call('/api/power', {on: 0})">Tắt</button>
    </div>
  </div>

  <div class="card">
    <h2>Nhiệt độ</h2>
    <p class="caption">Khoảng điều chỉnh 17°C – 30°C, mỗi lần ±1°C (không áp dụng ở chế độ Fan).</p>
    <div id="temp">-- °C</div>
    <div class="row">
      <button onclick="changeTemp(-1)">− 1°C</button>
      <button onclick="changeTemp(1)">+ 1°C</button>
    </div>
  </div>

  <div class="card">
    <h2>Chế độ</h2>
    <p class="caption">Auto: tự chọn chế độ theo nhiệt độ phòng. Cool: làm lạnh. Dry: hút ẩm, không ưu tiên làm lạnh sâu. Fan: chỉ chạy quạt, không làm lạnh.</p>
    <div class="row" id="modeRow"></div>
  </div>

  <div class="card">
    <h2>Tốc độ quạt</h2>
    <p class="caption">Auto: máy tự điều chỉnh tốc độ quạt theo chế độ đang chạy. Ở chế độ Auto và Dry máy không cho chọn quạt (nút bị khóa; mức đã chọn được giữ để dùng lại ở Cool hoặc Fan). Đổi tốc độ quạt sẽ tắt Sleep và Turbo, không tắt Eco. Bật Eco hoặc Sleep đặt quạt về Auto.</p>
    <div class="row" id="fanRow"></div>
  </div>

  <div class="card">
    <h2>Sleep (Ngủ)</h2>
    <p class="caption">Giảm dần tốc độ quạt và nới nhiệt độ theo thời gian ban đêm, giúp êm và tiết kiệm điện hơn khi ngủ. Chỉ bật được khi máy đang chạy ở chế độ Auto hoặc Cool. Bật Sleep đặt quạt về Auto. Sleep tự tắt khi đổi chế độ hoặc đổi tốc độ quạt (phải bật lại) và khi bật Eco, Turbo hoặc Clean. Bật/tắt Sleep không ảnh hưởng hẹn giờ.</p>
    <div class="row">
      <button id="btnSleepOn" onclick="call('/api/sleep', {on: 1})">Bật</button>
      <button id="btnSleepOff" onclick="call('/api/sleep', {on: 0})">Tắt</button>
    </div>
  </div>

  <div class="card">
    <h2>Eco (Tiết kiệm điện)</h2>
    <p class="caption">Chế độ tiết kiệm điện của remote. Chỉ bật được khi máy đang chạy ở chế độ Cool. Bật Eco đặt quạt về Auto. Đổi nhiệt độ hoặc tốc độ quạt vẫn giữ Eco. Eco tự tắt khi đổi chế độ, tắt máy hoặc bật Sleep, Turbo, Clean.</p>
    <div class="row">
      <button id="btnEcoOn" onclick="call('/api/eco', {on: 1})">Bật</button>
      <button id="btnEcoOff" onclick="call('/api/eco', {on: 0})">Tắt</button>
    </div>
  </div>

  <div class="card">
    <h2>Turbo</h2>
    <p class="caption">Làm lạnh nhanh hết công suất. Chỉ bật được khi máy đang chạy ở chế độ Cool. Turbo tự tắt khi đổi chế độ, đổi tốc độ quạt, tắt máy hoặc bật Sleep, Eco, Clean. Đổi nhiệt độ và đặt hẹn giờ vẫn giữ Turbo. Bật Turbo sẽ tắt Sleep và Eco, không đổi tốc độ quạt. Bật/tắt Turbo không ảnh hưởng hẹn giờ.</p>
    <div class="row">
      <button id="btnTurboOn" onclick="call('/api/turbo', {on: 1})">Bật</button>
      <button id="btnTurboOff" onclick="call('/api/turbo', {on: 0})">Tắt</button>
    </div>
  </div>

  <div class="card">
    <h2>Clean (Tự làm sạch)</h2>
    <p class="caption">Máy tự làm sạch dàn lạnh trong 20 phút rồi tự tắt. Chỉ bật được khi máy đang chạy. Bật Clean sẽ hủy hẹn giờ "Sau X giờ", Eco, Sleep và Turbo; hẹn giờ "Lúc hh:mm" do ESP32 giữ vẫn còn (hẹn Tắt vẫn chạy, hẹn Bật bị bỏ qua vì máy không nhận nút Bật khi đang Clean). Trong lúc Clean, máy chỉ nhận nút Clean (bấm Tắt để dừng, máy sẽ tắt hẳn), nút Nguồn (Tắt) và nút Light; các nút khác sẽ báo lỗi. Trang tự cập nhật mỗi 5 giây nên sẽ thấy máy tắt khi Clean xong.</p>
    <div class="row">
      <button id="btnCleanOn" onclick="call('/api/clean', {on: 1})">Bật</button>
      <button id="btnCleanOff" onclick="call('/api/clean', {on: 0})">Tắt</button>
    </div>
  </div>

  <div class="card">
    <h2>Light (Đèn hiển thị)</h2>
    <p class="caption">Bật/tắt đèn hiển thị trên dàn lạnh. Đây là nút "đảo trạng thái" giống remote thật, nên trang này không hiển thị trạng thái đèn. Bấm được cả khi máy đang tắt (máy chỉ bíp, đèn trạng thái không sáng vì máy tắt) và cả khi đang Clean.</p>
    <div class="row"><button onclick="call('/api/light', {})">Bấm để đảo Light</button></div>
  </div>

  <div class="card">
    <h2>Swing (Cánh đảo gió)</h2>
    <p class="caption">Bật/tắt dao động cánh gió lên-xuống. Đây là nút "đảo trạng thái" giống remote thật — mỗi lần bấm sẽ đảo ngược trạng thái hiện có trên máy. ESP32 không đọc được trạng thái thật trên máy nên trang này không hiển thị trạng thái Swing.</p>
    <div class="row"><button onclick="call('/api/swing', {})">Bấm để đảo Swing</button></div>
  </div>

  <div class="card">
    <h2>Hẹn giờ Bật / Tắt</h2>
    <p class="caption">Chọn một trong hai cách hẹn; đặt cách này thì cách kia bị hủy. Mỗi cách đặt được cả hẹn Bật lẫn hẹn Tắt. Chỉ khi bấm "Đặt hẹn giờ" lệnh mới được gửi. Đổi nhiệt độ/chế độ/quạt khi đang hẹn giờ vẫn giữ hẹn giờ.</p>
    <div class="row">
      <button id="modeDelayBtn" onclick="setTimerMode('delay')">Sau X giờ</button>
      <button id="modeClockBtn" onclick="setTimerMode('clock')">Lúc hh:mm</button>
    </div>

    <div id="delayPanel">
      <p class="caption" style="margin-top:12px">Máy lạnh tự đếm giờ nên ESP32 tắt cũng không ảnh hưởng. Bước 30 phút tới 10 giờ, bước 1 giờ từ 10 giờ tới 24 giờ. "Không hẹn" = không đặt. Đặt cùng lúc hẹn Bật và hẹn Tắt thì cả hai tính từ lúc bấm "Đặt hẹn giờ".</p>
      <label class="lbl" for="onTimerSel">Bật sau</label>
      <select id="onTimerSel"></select>
      <label class="lbl" for="offTimerSel">Tắt sau</label>
      <select id="offTimerSel"></select>
    </div>

    <div id="clockPanel" style="display:none">
      <p class="caption" style="margin-top:12px">ESP32 tự đếm theo giờ thực (UTC+7) và gửi lệnh Bật/Tắt đúng giờ. <b>ESP32 phải luôn bật tới giờ hẹn</b>: nếu bị tắt điện đúng lúc đó thì hẹn bị lỡ (trang sẽ báo). Hẹn một lần; giờ đã qua trong hôm nay thì hẹn sang ngày mai. Đánh dấu ô nào thì hẹn ô đó.</p>
      <div class="atrow">
        <label><input type="checkbox" id="onAtChk"> Bật lúc</label>
        <input type="time" id="onAtTime" value="06:00">
      </div>
      <div class="atrow">
        <label><input type="checkbox" id="offAtChk"> Tắt lúc</label>
        <input type="time" id="offAtTime" value="22:00">
      </div>
    </div>

    <div class="row timer-actions">
      <button onclick="applyTimer()">Đặt hẹn giờ</button>
      <button onclick="call('/api/cancel', {}, true, 'Đã hủy hẹn giờ.')">Hủy hẹn giờ</button>
    </div>
    <div class="timer-status" id="timerStatus"></div>
  </div>

  <div id="status">Đang tải trạng thái...</div>

<script>
  var MODES = [[0,'Auto'],[1,'Cool'],[2,'Dry'],[3,'Fan']];
  var FANS  = [[0,'Auto'],[1,'Thấp'],[2,'Vừa'],[3,'Cao']];
  var state = {power:false, mode:0, fan:0, temp:25, sleep:false, eco:false,
               turbo:false, clean:false, on_timer:0, off_timer:0,
               kind:0, clock:-1, on_left:0, off_left:0, on_due:-1, off_due:-1,
               missed_on:-1, missed_off:-1};
  var timerMode = 'delay';

  function pad2(n) { return (n < 10 ? '0' : '') + n; }
  function hhmm(m) { return pad2(Math.floor(m / 60)) + ':' + pad2(m % 60); }

  // Mốc hẹn giờ giống remote: 30 phút/bước tới 10 giờ, rồi 1 giờ/bước tới 24 giờ.
  function timerLabel(m) {
    var h = Math.floor(m / 60), mm = m % 60;
    return (h > 0 ? h + ' giờ' : '') + (h > 0 && mm > 0 ? ' ' : '') + (mm > 0 ? mm + ' phút' : '');
  }
  function fillTimerSelect(sel) {
    sel.innerHTML = '';
    var opts = [0];
    var m;
    for (m = 30; m <= 600; m += 30) opts.push(m);
    for (m = 660; m <= 1440; m += 60) opts.push(m);
    opts.forEach(function(v) {
      var o = document.createElement('option');
      o.value = String(v);
      o.innerText = v === 0 ? 'Không hẹn' : timerLabel(v);
      sel.appendChild(o);
    });
  }

  function setStatus(text) { document.getElementById('status').innerText = text; }

  function render() {
    document.getElementById('temp').innerText = state.mode === 3 ? '--' : state.temp + ' °C';
    document.getElementById('btnOn').className = state.power ? 'power-on' : '';
    document.getElementById('btnOff').className = !state.power ? 'power-off' : '';
    document.getElementById('btnSleepOn').className = state.sleep ? 'active' : '';
    document.getElementById('btnSleepOff').className = !state.sleep ? 'active' : '';
    document.getElementById('btnEcoOn').className = state.eco ? 'active' : '';
    document.getElementById('btnEcoOff').className = !state.eco ? 'active' : '';
    document.getElementById('btnTurboOn').className = state.turbo ? 'active' : '';
    document.getElementById('btnTurboOff').className = !state.turbo ? 'active' : '';
    document.getElementById('btnCleanOn').className = state.clean ? 'active' : '';
    document.getElementById('btnCleanOff').className = !state.clean ? 'active' : '';
    // Sleep chỉ có ở Auto (0) và Cool (1); Eco và Turbo chỉ có ở Cool (1).
    document.getElementById('btnSleepOn').disabled = state.mode !== 0 && state.mode !== 1;
    document.getElementById('btnEcoOn').disabled = state.mode !== 1;
    document.getElementById('btnTurboOn').disabled = state.mode !== 1;
    document.getElementById('btnCleanOn').disabled = !state.power;

    var modeRow = document.getElementById('modeRow');
    modeRow.innerHTML = '';
    MODES.forEach(function(m) {
      var b = document.createElement('button');
      b.innerText = m[1];
      if (state.mode === m[0]) b.className = 'active';
      b.onclick = function() { setClimate({mode: m[0]}); };
      modeRow.appendChild(b);
    });

    // Auto (0) và Dry (2): remote không cho chọn quạt. Vẫn tô sáng mức đã lưu (dùng lại ở Cool/Fan).
    var fanLocked = state.mode === 0 || state.mode === 2;
    var fanRow = document.getElementById('fanRow');
    fanRow.innerHTML = '';
    FANS.forEach(function(f) {
      var b = document.createElement('button');
      b.innerText = f[1];
      if (state.fan === f[0]) b.className = 'active';
      b.disabled = fanLocked;
      b.onclick = function() { setClimate({fan: f[0]}); };
      fanRow.appendChild(b);
    });

    renderTimerStatus();
  }

  // ---- Hẹn giờ ---------------------------------------------------------------

  function setTimerMode(mode) {
    timerMode = mode;
    document.getElementById('delayPanel').style.display = mode === 'delay' ? '' : 'none';
    document.getElementById('clockPanel').style.display = mode === 'clock' ? '' : 'none';
    document.getElementById('modeDelayBtn').className = mode === 'delay' ? 'active' : '';
    document.getElementById('modeClockBtn').className = mode === 'clock' ? 'active' : '';
  }

  // Đưa các ô nhập hẹn giờ về đúng hẹn giờ đang có trên ESP32. Chỉ gọi lúc tải trang, sau khi
  // bấm Đặt/Hủy, và khi hẹn giờ trên ESP32 đổi (tới hạn) — KHÔNG gọi ở mỗi lần tự cập nhật,
  // để không ghi đè ô người dùng đang chọn dở.
  function syncTimerInputs() {
    document.getElementById('onTimerSel').value = String(state.on_timer);
    document.getElementById('offTimerSel').value = String(state.off_timer);
    var isClock = state.kind === 2;
    var onSet = isClock && state.on_due >= 0;
    var offSet = isClock && state.off_due >= 0;
    document.getElementById('onAtChk').checked = onSet;
    document.getElementById('offAtChk').checked = offSet;
    if (onSet) document.getElementById('onAtTime').value = hhmm(state.on_due);
    if (offSet) document.getElementById('offAtTime').value = hhmm(state.off_due);
    if (state.kind === 1) setTimerMode('delay');
    if (state.kind === 2) setTimerMode('clock');
  }

  // Phút trong ngày của một ô giờ. -1: ô không được đánh dấu. null: đánh dấu mà chưa chọn giờ.
  function clockValue(chkId, timeId) {
    if (!document.getElementById(chkId).checked) return -1;
    var v = document.getElementById(timeId).value;
    if (!/^\d\d:\d\d/.test(v)) return null;
    return parseInt(v.slice(0, 2), 10) * 60 + parseInt(v.slice(3, 5), 10);
  }

  function applyTimer() {
    if (timerMode === 'delay') {
      call('/api/timer', {
        on: parseInt(document.getElementById('onTimerSel').value),
        off: parseInt(document.getElementById('offTimerSel').value)
      }, true);
      return;
    }
    var on = clockValue('onAtChk', 'onAtTime');
    var off = clockValue('offAtChk', 'offAtTime');
    if (on === null || off === null) { setStatus('Lỗi: hãy chọn giờ cho ô đã đánh dấu.'); return; }
    call('/api/schedule', {on: on, off: off}, true,
         'Đã đặt hẹn giờ. ESP32 sẽ gửi lệnh đúng giờ (phải luôn bật).');
  }

  function leftText(sec) {
    var m = Math.ceil(sec / 60);
    return m < 1 ? 'dưới 1 phút' : timerLabel(m);
  }

  function addLine(box, text, cls) {
    var d = document.createElement('div');
    if (cls) d.className = cls;
    d.innerText = text;
    box.appendChild(d);
  }

  // Dòng trạng thái hẹn giờ. Mọi số liệu do ESP32 tính, không phụ thuộc đồng hồ trình duyệt.
  function renderTimerStatus() {
    var box = document.getElementById('timerStatus');
    box.innerHTML = '';
    if (state.clock >= 0) {
      addLine(box, 'Giờ ESP32: ' + hhmm(state.clock) + ' (UTC+7)', 'dim');
    } else {
      addLine(box, 'ESP32 chưa đồng bộ giờ (cần Internet): chưa đặt được hẹn giờ.', 'warn');
    }
    if (state.kind === 0) {
      addLine(box, 'Chưa đặt hẹn giờ.', 'dim');
    } else if (state.clock < 0) {
      addLine(box, 'Đang có hẹn giờ nhưng ESP32 chưa có giờ nên chưa tính được thời gian còn lại.', 'dim');
    } else {
      if (state.on_due >= 0) addLine(box, 'Bật sau ' + leftText(state.on_left) + ' (lúc ' + hhmm(state.on_due) + ')');
      if (state.off_due >= 0) addLine(box, 'Tắt sau ' + leftText(state.off_left) + ' (lúc ' + hhmm(state.off_due) + ')');
    }
    var why = ' không được thực hiện (ESP32 bị tắt hoặc chưa có giờ đúng lúc đó, hoặc máy đang Clean). ' +
              'Trạng thái nguồn trên trang có thể lệch máy thật.';
    if (state.missed_on >= 0) addLine(box, 'Cảnh báo: hẹn Bật ' + hhmm(state.missed_on) + why, 'warn');
    if (state.missed_off >= 0) addLine(box, 'Cảnh báo: hẹn Tắt ' + hhmm(state.missed_off) + why, 'warn');
  }

  // ---- Gọi ESP32 --------------------------------------------------------------

  // Gọi một endpoint của ESP32. Thành công: server trả state mới. Lỗi 400: hiện nguyên văn lý do.
  // sync = true: đưa các ô nhập hẹn giờ về đúng hẹn giờ vừa đặt/hủy.
  function call(path, params, sync, okMsg) {
    var qs = new URLSearchParams(params).toString();
    setStatus('Đang gửi lệnh...');
    fetch(path + (qs ? '?' + qs : ''))
      .then(function(r) {
        if (!r.ok) return r.text().then(function(t) { throw new Error(t); });
        return r.json();
      })
      .then(function(s) {
        state = s; render();
        if (sync) syncTimerInputs();
        setStatus(okMsg || 'Đã gửi lệnh IR.');
      })
      .catch(function(e) {
        setStatus(e instanceof TypeError ? 'Lỗi kết nối tới ESP32.' : 'Lỗi: ' + e.message);
      });
  }

  function setClimate(patch) {
    var c = {mode: state.mode, fan: state.fan, temp: state.temp};
    for (var k in patch) c[k] = patch[k];
    call('/api/climate', c);
  }

  function changeTemp(delta) {
    var t = state.temp + delta;
    if (t < 17 || t > 30) return;
    setClimate({temp: t});
  }

  // ---- Tự cập nhật mỗi 5 giây (khi tab đang hiển thị) --------------------------

  // Phần state làm đổi giao diện điều khiển; chỉ vẽ lại các nút khi phần này đổi, để không
  // dựng lại nút mỗi 5 giây (dễ làm mất cú bấm).
  function controlSig(s) {
    return [s.power, s.mode, s.fan, s.temp, s.sleep, s.eco, s.turbo, s.clean].join();
  }
  // Hẹn giờ đổi (tới hạn, hoặc đặt từ thiết bị khác) thì đưa ô nhập về đúng.
  function timerSig(s) {
    return [s.kind, s.on_timer, s.off_timer, s.on_due, s.off_due].join();
  }

  function poll() {
    fetch('/api/state')
      .then(function(r) { return r.json(); })
      .then(function(s) {
        var controlChanged = controlSig(s) !== controlSig(state);
        var timerChanged = timerSig(s) !== timerSig(state);
        state = s;
        if (controlChanged) render(); else renderTimerStatus();
        if (timerChanged) syncTimerInputs();
      })
      .catch(function() { setStatus('Lỗi kết nối tới ESP32.'); });
  }
  setInterval(function() { if (!document.hidden) poll(); }, 5000);

  function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    document.getElementById('themeBtn').innerText = theme === 'dark' ? '🌙' : '☀️';
  }
  function toggleTheme() {
    var cur = document.documentElement.getAttribute('data-theme') || 'dark';
    var next = cur === 'dark' ? 'light' : 'dark';
    applyTheme(next);
    try { localStorage.setItem('theme', next); } catch (e) {}
  }
  (function initTheme() {
    var saved = 'dark';
    try { saved = localStorage.getItem('theme') || 'dark'; } catch (e) {}
    applyTheme(saved);
  })();

  fillTimerSelect(document.getElementById('onTimerSel'));
  fillTimerSelect(document.getElementById('offTimerSel'));
  setTimerMode('delay');

  fetch('/api/state').then(function(r) { return r.json(); }).then(function(s) {
    state = s; render(); syncTimerInputs(); setStatus('Sẵn sàng.');
  }).catch(function() { setStatus('Lỗi kết nối tới ESP32.'); });
</script>
</body>
</html>
)HTMLPAGE";
