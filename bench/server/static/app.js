const bench = (() => {
  const f2 = x => (x >= 0 ? '+' : '') + x.toFixed(2);
  const f1 = x => (x >= 0 ? '+' : '') + x.toFixed(1);

  function setField(scope, name, html) {
    const el = scope.querySelector(`[data-f="${name}"]`);
    if (el) el.innerHTML = html;
  }

  function rowFrom(t) {
    return {
      llr: `${t.llr.toFixed(2)} <span class="muted">(${t.llr_lower.toFixed(2)}, ${t.llr_upper.toFixed(2)})</span>`,
      elo: `${f1(t.elo)} ± ${((t.elo_hi - t.elo_lo) / 2).toFixed(1)}`,
      games: t.games,
      status: t.status,
      result: t.result ? '· ' + t.result : '',
    };
  }

  function pollTests() {
    async function tick() {
      try {
        const tests = await (await fetch('/api/tests')).json();
        for (const t of tests) {
          const tr = document.querySelector(`tr[data-id="${t.id}"]`);
          if (!tr) continue;
          const r = rowFrom(t);
          setField(tr, 'llr', r.llr);
          setField(tr, 'elo', r.elo);
          setField(tr, 'games', r.games);
          const st = tr.querySelector('[data-f="status"]');
          if (st) { st.textContent = t.status; st.className = 'status ' + t.status; }
          const bar = tr.querySelector('[data-f="progress"]');
          if (bar) bar.style.width = t.progress + '%';
        }
      } catch (e) { }
    }
    tick(); setInterval(tick, 2500);
  }

  function csrfToken() {
    const m = document.querySelector('meta[name="csrf-token"]');
    return m ? m.getAttribute('content') : '';
  }

  function pollTest(id) {
    const stopBtn = document.getElementById('stopBtn');
    if (stopBtn) stopBtn.addEventListener('click', async () => {
      stopBtn.disabled = true;
      await fetch(`/api/test/${id}/stop`, {
        method: 'POST',
        headers: { 'X-CSRF-Token': csrfToken() },
      });
    });
    async function tick() {
      try {
        const t = await (await fetch(`/api/test/${id}`)).json();
        const s = document;
        setField(s, 'llr', t.llr.toFixed(2));
        setField(s, 'elo', f1(t.elo));
        setField(s, 'elorange', `95% CI [${f1(t.elo_lo)}, ${f1(t.elo_hi)}]`);
        setField(s, 'los', (t.los * 100).toFixed(1) + '%');
        setField(s, 'games', t.games);
        setField(s, 'wdl', `${t.w} W · ${t.d} D · ${t.l} L`);
        const st = s.querySelector('[data-f="status"]');
        if (st) { st.textContent = t.status; st.className = 'status ' + t.status; }
        const rs = s.querySelector('[data-f="result"]');
        if (rs && t.result) { rs.textContent = '· ' + t.result; rs.className = 'result ' + t.result; }
        const bar = s.querySelector('[data-f="progress"]');
        if (bar) bar.style.width = t.progress + '%';
        const penta = s.querySelector('[data-f="penta"]');
        if (penta) {
          const labels = ['0–2 (LL)', '½', '1 (draw)', '1½', '2 (WW)'];
          penta.innerHTML = t.penta.map((c, i) => `<div><b>${c}</b><small>${labels[i]}</small></div>`).join('');
        }
        if (stopBtn && ['finished', 'stopped', 'failed'].includes(t.status)) stopBtn.style.display = 'none';
      } catch (e) { }
    }
    tick(); setInterval(tick, 2000);
  }

  return { pollTests, pollTest };
})();
