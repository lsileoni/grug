(() => {
  const pieces = {
    K: '♔', Q: '♕', R: '♖', B: '♗', N: '♘', P: '♙',
    k: '♚', q: '♛', r: '♜', b: '♝', n: '♞', p: '♟',
  };

  function boardFromFen(fen) {
    const board = [];
    for (const rank of fen.split(' ')[0].split('/')) {
      const row = [];
      for (const ch of rank) {
        if (/\d/.test(ch)) {
          for (let i = 0; i < Number(ch); i++) row.push('');
        } else {
          row.push(ch);
        }
      }
      board.push(row);
    }
    return board;
  }

  function render(viewer) {
    const positions = JSON.parse(viewer.dataset.positions || '[]');
    if (!positions.length) return;
    let index = Number(viewer.dataset.index || 0);
    index = Math.max(0, Math.min(index, positions.length - 1));
    viewer.dataset.index = String(index);

    const pos = positions[index];
    const boardEl = viewer.querySelector('[data-board]');
    boardEl.innerHTML = '';
    boardFromFen(pos.fen).forEach((row, r) => {
      row.forEach((piece, f) => {
        const sq = document.createElement('div');
        sq.className = 'square ' + (((r + f) % 2) ? 'dark' : 'light');
        sq.textContent = pieces[piece] || '';
        boardEl.appendChild(sq);
      });
    });

    viewer.querySelector('[data-ply]').textContent =
      `${index + 1}/${positions.length} · ply ${pos.ply} · ${pos.move}`;
    viewer.querySelector('[data-fen]').textContent = pos.fen;
  }

  function setIndex(viewer, fn) {
    const positions = JSON.parse(viewer.dataset.positions || '[]');
    const current = Number(viewer.dataset.index || 0);
    viewer.dataset.index = String(fn(current, positions.length));
    render(viewer);
  }

  document.querySelectorAll('.game-viewer').forEach(viewer => {
    viewer.dataset.index = '0';
    viewer.querySelector('[data-first]').addEventListener('click', () => setIndex(viewer, () => 0));
    viewer.querySelector('[data-prev]').addEventListener('click', () => setIndex(viewer, i => i - 1));
    viewer.querySelector('[data-next]').addEventListener('click', () => setIndex(viewer, i => i + 1));
    viewer.querySelector('[data-last]').addEventListener('click', () => setIndex(viewer, (i, n) => n - 1));
    viewer.addEventListener('toggle', () => {
      if (viewer.open) render(viewer);
    });
  });
})();
