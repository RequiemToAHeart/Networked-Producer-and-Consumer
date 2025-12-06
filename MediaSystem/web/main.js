async function fetchList() {
  const res = await fetch('/api/list');
  if (!res.ok) return [];
  const arr = await res.json();
  return arr;
}

function makeCard(item) {
  const card = document.createElement('div');
  card.className = 'card';
  const v = document.createElement('video');
  v.src = item.preview;
  v.muted = true;
  v.playsInline = true;
  v.onmouseenter = () => { v.play().catch(()=>{}); };
  v.onmouseleave = () => { v.pause(); v.currentTime = 0; };
  v.onclick = () => {
    const modal = document.createElement('div');
    modal.className = 'modal';
    modal.onclick = () => document.body.removeChild(modal);
    const full = document.createElement('video');
    full.controls = true;
    full.autoplay = true;
    full.src = item.url;
    modal.appendChild(full);
    document.body.appendChild(modal);
  };
  card.appendChild(v);
  const p = document.createElement('div');
  p.textContent = item.filename;
  card.appendChild(p);
  return card;
}

async function refresh() {
  const grid = document.getElementById('grid');
  grid.innerHTML = '';
  const list = await fetchList();
  for (let it of list) {
    const parsed = JSON.parse(it); // because server returns array of JSON lines we assembled earlier
    // parsed contains filename, preview, url
    grid.appendChild(makeCard(parsed));
  }
}

setInterval(refresh, 2000);
refresh();
