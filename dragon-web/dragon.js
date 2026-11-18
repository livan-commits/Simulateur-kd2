/*!
 * FireDragon — animation de dragon interactif en Canvas 2D (vanilla JS, zéro dépendance).
 * Usage : const dragon = new FireDragon(canvas, { ...options }); dragon.start();
 */
(function (global) {
  'use strict';

  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function angDiff(a, b) {
    let d = (b - a) % (Math.PI * 2);
    if (d > Math.PI) d -= Math.PI * 2;
    if (d < -Math.PI) d += Math.PI * 2;
    return d;
  }
  function hexToRgba(hex, alpha) {
    let h = hex.replace('#', '');
    if (h.length === 3) h = h[0] + h[0] + h[1] + h[1] + h[2] + h[2];
    const n = parseInt(h, 16);
    return 'rgba(' + ((n >> 16) & 255) + ',' + ((n >> 8) & 255) + ',' + (n & 255) + ',' + alpha + ')';
  }

  class FireDragon {
    constructor(canvas, opts) {
      const o = opts || {};
      this.canvas = canvas;
      this.ctx = canvas.getContext('2d');

      this.scale = o.scale != null ? o.scale : 1;
      this.maxRadius = (o.radius != null ? o.radius : 14) * this.scale;
      this.spacing = (o.spacing != null ? o.spacing : this.maxRadius * 0.72) * this.scale;
      this.count = o.segments != null ? o.segments : 32;
      this.speed = (o.speed != null ? o.speed : 300) * this.scale;
      this.turnRate = o.turnRate != null ? o.turnRate : 3.2;
      this.flapSpeed = o.flapSpeed != null ? o.flapSpeed : 6.5;
      this.wiggleFreq = o.wiggleFreq != null ? o.wiggleFreq : 3.2;
      this.wiggleAmp = (o.wiggleAmp != null ? o.wiggleAmp : 10) * this.scale;

      this.bodyColor = o.bodyColor || '#b3202c';
      this.spineColor = o.spineColor || '#7a1220';
      this.hornColor = o.hornColor || '#e8c76b';
      this.eyeColor = o.eyeColor || '#ffd34d';
      this.fireHue = o.fireHue != null ? o.fireHue : 45;

      this.autoFlight = !!o.autoFlight;
      this.controlWindow = o.controlWindow !== false;
      this.orbitRadius = this.maxRadius * 6;

      this.t = 0;
      this.w = 0;
      this.h = 0;
      this.head = { x: 0, y: 0, a: 0 };
      this.target = { x: 0, y: 0 };
      this.chain = [];
      this.pts = [];
      this.breathing = false;
      this.jaw = 0;
      this.flames = [];
      this.smoke = [];
      this._raf = 0;
      this._last = 0;
      this._running = false;
      this._handlers = null;

      this._resize();
      this._initChain();
    }

    /* ---------- API publique ---------- */

    start() {
      if (this._running) return;
      this._running = true;
      this._bindInput();
      this._last = 0;
      const loop = (ts) => {
        if (!this._running) return;
        if (!this._last) this._last = ts;
        const dt = Math.min(0.05, (ts - this._last) / 1000);
        this._last = ts;
        this.update(dt);
        this.draw();
        this._raf = global.requestAnimationFrame(loop);
      };
      this._raf = global.requestAnimationFrame(loop);
    }

    stop() {
      this._running = false;
      if (this._raf && global.cancelAnimationFrame) global.cancelAnimationFrame(this._raf);
      this._raf = 0;
      this._unbindInput();
    }

    destroy() { this.stop(); }

    setTarget(x, y) { this.target.x = x; this.target.y = y; }

    setAutoFlight(on) { this.autoFlight = !!on; }

    isAutoFlight() { return this.autoFlight; }

    breathe(on) { this.breathing = !!on; }

    /* Positions rendues du corps (pts[0] = tête) — pratique pour le gameplay/collisions. */
    getSegments() { return this.pts; }

    /* Renvoie true si le point (x, y) touche le dragon. */
    hitTest(x, y, pad) {
      const p = (pad != null ? pad : this.maxRadius * 0.4) * this.scale;
      for (let i = 0; i < this.pts.length; i++) {
        const s = this.pts[i];
        const dx = x - s.x, dy = y - s.y;
        if (dx * dx + dy * dy < (s.r + p) * (s.r + p)) return true;
      }
      return false;
    }

    /* ---------- Boucle ---------- */

    update(dt) {
      this.t += dt;
      this._steer(dt);
      this._follow();
      this._updateParticles(dt);
      const jawTarget = this.breathing ? 1 : 0;
      this.jaw += (jawTarget - this.jaw) * clamp(dt * 12, 0, 1);
    }

    draw() {
      const ctx = this.ctx;
      ctx.clearRect(0, 0, this.w, this.h);
      this._drawShadow(ctx);
      this._drawWings(ctx);
      this._drawBody(ctx);
      this._drawHead(ctx);
      this._drawParticles(ctx);
    }

    /* ---------- Déplacement ---------- */

    _steer(dt) {
      if (this.autoFlight || (this.target.x === 0 && this.target.y === 0)) {
        const t = this.t;
        this.target.x = this.w / 2 + Math.sin(t * 0.55) * this.w * 0.34 + Math.sin(t * 1.3) * this.w * 0.06;
        this.target.y = this.h / 2 + Math.sin(t * 1.1) * this.h * 0.30;
      }
      this.target.x = clamp(this.target.x, this.maxRadius * 3, this.w - this.maxRadius * 3);
      this.target.y = clamp(this.target.y, this.maxRadius * 3, this.h - this.maxRadius * 3);

      const dx = this.target.x - this.head.x;
      const dy = this.target.y - this.head.y;
      const d = Math.hypot(dx, dy) || 1;
      const toTarget = Math.atan2(dy, dx);

      let desired = toTarget;
      if (d < this.orbitRadius) {
        const k = 1 - d / this.orbitRadius;
        desired += Math.PI / 2 * k;
      }
      const diff = angDiff(this.head.a, desired);
      this.head.a += clamp(diff, -this.turnRate * dt, this.turnRate * dt);

      const boost = this.breathing ? 1.18 : 1;
      const sp = this.speed * boost * clamp(d / this.orbitRadius, 0.35, 1);
      this.head.x += Math.cos(this.head.a) * sp * dt;
      this.head.y += Math.sin(this.head.a) * sp * dt;
      this.head.x = clamp(this.head.x, -this.maxRadius * 4, this.w + this.maxRadius * 4);
      this.head.y = clamp(this.head.y, -this.maxRadius * 4, this.h + this.maxRadius * 4);
    }

    _follow() {
      this.chain[0].x = this.head.x;
      this.chain[0].y = this.head.y;
      for (let i = 1; i < this.count; i++) {
        const p = this.chain[i], prev = this.chain[i - 1];
        let dx = prev.x - p.x, dy = prev.y - p.y;
        let d = Math.hypot(dx, dy);
        if (d < 0.0001) { dx = 1; dy = 0; d = 1; }
        p.x = prev.x - (dx / d) * this.spacing;
        p.y = prev.y - (dy / d) * this.spacing;
        p.a = Math.atan2(dy, dx);
      }

      const amp = this.wiggleAmp * (this.breathing ? 0.6 : 1);
      for (let i = 0; i < this.count; i++) {
        const c = this.chain[i];
        const k = Math.pow(i / this.count, 1.3);
        const off = Math.sin(this.t * this.wiggleFreq - i * 0.55) * amp * k;
        const out = this.pts[i];
        out.x = c.x + Math.cos(c.a + Math.PI / 2) * off;
        out.y = c.y + Math.sin(c.a + Math.PI / 2) * off;
        out.a = c.a;
        out.r = this.maxRadius * (1 - 0.78 * Math.pow(i / (this.count - 1), 1.4));
      }
      for (let i = 1; i < this.count; i++) {
        this.pts[i].a = Math.atan2(this.pts[i - 1].y - this.pts[i].y, this.pts[i - 1].x - this.pts[i].x);
      }
    }

    _initChain() {
      this.chain = [];
      this.pts = [];
      for (let i = 0; i < this.count; i++) {
        this.chain.push({ x: this.w / 2 - i * this.spacing, y: this.h / 2, a: 0 });
        this.pts.push({ x: 0, y: 0, a: 0, r: this.maxRadius });
      }
      this.head.x = this.w / 2;
      this.head.y = this.h / 2;
      this.head.a = 0;
      this.target.x = this.w / 2;
      this.target.y = this.h / 2;
      this._follow();
    }

    _resize() {
      let w = 800, h = 600;
      if (typeof global.innerWidth === 'number') { w = global.innerWidth; h = global.innerHeight; }
      if (this.canvas.clientWidth && this.canvas.clientHeight) {
        w = this.canvas.clientWidth;
        h = this.canvas.clientHeight;
      }
      let dpr = 1;
      if (global.devicePixelRatio) dpr = Math.min(global.devicePixelRatio, 2);
      this.canvas.width = Math.round(w * dpr);
      this.canvas.height = Math.round(h * dpr);
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      this.w = w;
      this.h = h;
    }

    /* ---------- Entrées ---------- */

    _bindInput() {
      if (!this.controlWindow || typeof global.addEventListener !== 'function') return;
      const move = (e) => { this.setAutoFlight(false); this.setTarget(e.clientX, e.clientY); };
      const down = () => this.breathe(true);
      const up = () => this.breathe(false);
      const key = (e) => {
        if (e.code === 'Space') { this.breathe(true); e.preventDefault(); }
        if (e.key === 'a' || e.key === 'A') this.setAutoFlight(!this.autoFlight);
      };
      const keyUp = (e) => { if (e.code === 'Space') this.breathe(false); };
      const resize = () => { this._resize(); };
      global.addEventListener('pointermove', move);
      global.addEventListener('pointerdown', down);
      global.addEventListener('pointerup', up);
      global.addEventListener('keydown', key);
      global.addEventListener('keyup', keyUp);
      global.addEventListener('resize', resize);
      this._handlers = { move, down, up, key, keyUp, resize };
    }

    _unbindInput() {
      if (!this._handlers) return;
      const h = this._handlers;
      global.removeEventListener('pointermove', h.move);
      global.removeEventListener('pointerdown', h.down);
      global.removeEventListener('pointerup', h.up);
      global.removeEventListener('keydown', h.key);
      global.removeEventListener('keyup', h.keyUp);
      global.removeEventListener('resize', h.resize);
      this._handlers = null;
    }

    /* ---------- Particules (feu / fumée) ---------- */

    _mouth() {
      const h = this.pts[0];
      const m = this.maxRadius * 2.6;
      return {
        x: h.x + Math.cos(h.a) * m,
        y: h.y + Math.sin(h.a) * m,
        a: h.a
      };
    }

    _updateParticles(dt) {
      if (this.breathing) {
        const m = this._mouth();
        const vBase = this.speed * 1.1;
        for (let i = 0; i < 4; i++) {
          const spread = (Math.random() - 0.5) * 0.45;
          const a = m.a + spread;
          const sp = vBase * (0.55 + Math.random() * 0.7);
          this.flames.push({
            x: m.x + (Math.random() - 0.5) * this.maxRadius,
            y: m.y + (Math.random() - 0.5) * this.maxRadius,
            vx: Math.cos(a) * sp,
            vy: Math.sin(a) * sp,
            life: 0.35 + Math.random() * 0.4,
            age: 0,
            r: this.maxRadius * (0.35 + Math.random() * 0.35),
            seed: Math.random() * 10
          });
        }
      }

      let i = this.flames.length;
      while (i--) {
        const p = this.flames[i];
        p.age += dt;
        if (p.age >= p.life) {
          this.flames.splice(i, 1);
          if (Math.random() < 0.5) {
            this.smoke.push({
              x: p.x, y: p.y,
              vx: p.vx * 0.12, vy: p.vy * 0.12,
              life: 0.8 + Math.random() * 0.8, age: 0,
              r: p.r * (0.8 + Math.random()),
              seed: Math.random() * 10
            });
          }
          continue;
        }
        const k = 1 - p.age / p.life;
        p.vx *= (1 - dt * 2.2);
        p.vy *= (1 - dt * 2.2);
        const curl = Math.sin(this.t * 12 + p.seed) * 60 * dt;
        p.x += p.vx * dt + curl * 0.3;
        p.y += p.vy * dt + curl;
        p.k = k;
      }

      i = this.smoke.length;
      while (i--) {
        const p = this.smoke[i];
        p.age += dt;
        if (p.age >= p.life) { this.smoke.splice(i, 1); continue; }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vx *= (1 - dt * 1.5);
        p.vy *= (1 - dt * 1.5);
        p.k = 1 - p.age / p.life;
      }
    }

    _drawParticles(ctx) {
      let i;
      for (i = 0; i < this.smoke.length; i++) {
        const p = this.smoke[i];
        const a = p.k * 0.16;
        ctx.fillStyle = 'rgba(90,90,95,' + a.toFixed(3) + ')';
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.r * (1.6 + (1 - p.k) * 2.2), 0, Math.PI * 2);
        ctx.fill();
      }

      ctx.globalCompositeOperation = 'lighter';
      for (i = 0; i < this.flames.length; i++) {
        const p = this.flames[i];
        const k = p.k;
        const hue = this.fireHue - 38 * (1 - k);
        const light = 50 + 15 * k;
        const r = p.r * (0.5 + 1.1 * (1 - Math.abs(k - 0.45) * 1.4));
        ctx.fillStyle = 'hsla(' + hue.toFixed(0) + ',100%,' + light.toFixed(0) + '%,' + (0.75 * k).toFixed(3) + ')';
        ctx.beginPath();
        ctx.arc(p.x, p.y, Math.max(0.5, r), 0, Math.PI * 2);
        ctx.fill();
      }
      const m = this._mouth();
      if (this.breathing) {
        const g = ctx.createRadialGradient(m.x, m.y, 0, m.x, m.y, this.maxRadius * 3.2);
        g.addColorStop(0, 'rgba(255,190,60,0.35)');
        g.addColorStop(1, 'rgba(255,120,20,0)');
        ctx.fillStyle = g;
        ctx.beginPath();
        ctx.arc(m.x, m.y, this.maxRadius * 3.2, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.globalCompositeOperation = 'source-over';
    }

    /* ---------- Rendu du dragon ---------- */

    _drawShadow(ctx) {
      ctx.fillStyle = 'rgba(0,0,0,0.16)';
      const ox = 26 * this.scale, oy = 34 * this.scale;
      for (let i = 0; i < this.count; i += 2) {
        const p = this.pts[i];
        ctx.beginPath();
        ctx.arc(p.x + ox, p.y + oy, p.r * 0.9, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    _drawWings(ctx) {
      const b = this.pts[5];
      if (!b) return;
      const flapPhase = this.t * this.flapSpeed;
      const lenScale = 0.72 + 0.28 * Math.cos(flapPhase);
      const sweep = Math.sin(flapPhase - 0.9) * 0.22;
      const len = this.maxRadius * 7.2 * lenScale;
      const membrane = hexToRgba(this.bodyColor, 0.82);
      const bone = hexToRgba(this.spineColor, 0.95);

      for (let side = -1; side <= 1; side += 2) {
        const base = b.a + side * Math.PI / 2 + sweep * side;
        const fingers = [];
        for (let f = 0; f < 3; f++) {
          const fa = base + side * (-0.15 - f * 0.5);
          const fl = len * (1 - f * 0.28);
          fingers.push({ x: b.x + Math.cos(fa) * fl, y: b.y + Math.sin(fa) * fl });
        }
        ctx.beginPath();
        ctx.moveTo(b.x, b.y);
        ctx.lineTo(fingers[0].x, fingers[0].y);
        for (let f = 0; f < fingers.length - 1; f++) {
          const t0 = fingers[f], t1 = fingers[f + 1];
          const mx = (t0.x + t1.x) / 2, my = (t0.y + t1.y) / 2;
          const cx = mx + (b.x - mx) * 0.3, cy = my + (b.y - my) * 0.3;
          ctx.quadraticCurveTo(cx, cy, t1.x, t1.y);
        }
        ctx.quadraticCurveTo(b.x - Math.cos(base) * len * 0.2, b.y - Math.sin(base) * len * 0.2, b.x, b.y);
        ctx.closePath();
        ctx.fillStyle = membrane;
        ctx.fill();

        ctx.strokeStyle = bone;
        ctx.lineCap = 'round';
        for (let f = 0; f < fingers.length; f++) {
          ctx.lineWidth = this.maxRadius * (0.34 - f * 0.07);
          ctx.beginPath();
          ctx.moveTo(b.x, b.y);
          ctx.lineTo(fingers[f].x, fingers[f].y);
          ctx.stroke();
        }
      }
    }

    _drawBody(ctx) {
      const ctxAny = ctx;
      for (let i = this.count - 1; i >= 0; i--) {
        const p = this.pts[i];
        const r = p.r;
        const grad = ctxAny.createRadialGradient(
          p.x + Math.cos(p.a) * r * 0.4, p.y + Math.sin(p.a) * r * 0.4, r * 0.1,
          p.x, p.y, r * 1.15
        );
        grad.addColorStop(0, hexToRgba(this.bodyColor, 1));
        grad.addColorStop(0.55, hexToRgba(this.bodyColor, 1));
        grad.addColorStop(1, hexToRgba(this.spineColor, 1));
        ctx.fillStyle = grad;
        ctx.beginPath();
        ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
        ctx.fill();
      }

      ctx.strokeStyle = hexToRgba(this.spineColor, 1);
      ctx.lineCap = 'round';
      for (let i = 2; i < this.count - 3; i += 2) {
        const p = this.pts[i];
        const r = p.r;
        const back = p.a + Math.PI;
        const w = r * 0.55;
        ctx.fillStyle = hexToRgba(this.spineColor, 0.95);
        ctx.beginPath();
        ctx.moveTo(p.x + Math.cos(back) * r * 1.5, p.y + Math.sin(back) * r * 1.5);
        ctx.lineTo(p.x + Math.cos(p.a + Math.PI / 2) * w, p.y + Math.sin(p.a + Math.PI / 2) * w);
        ctx.lineTo(p.x + Math.cos(p.a - Math.PI / 2) * w, p.y + Math.sin(p.a - Math.PI / 2) * w);
        ctx.closePath();
        ctx.fill();
      }

      const tail = this.pts[this.count - 1];
      const tc = this.pts[this.count - 3];
      const ta = Math.atan2(tail.y - tc.y, tail.x - tc.x);
      const tl = this.maxRadius * 2.6;
      ctx.fillStyle = hexToRgba(this.bodyColor, 0.95);
      ctx.beginPath();
      ctx.moveTo(tail.x, tail.y);
      ctx.quadraticCurveTo(
        tail.x + Math.cos(ta + 0.9) * tl, tail.y + Math.sin(ta + 0.9) * tl,
        tail.x + Math.cos(ta + 0.35) * tl * 1.35, tail.y + Math.sin(ta + 0.35) * tl * 1.35
      );
      ctx.quadraticCurveTo(
        tail.x + Math.cos(ta) * tl * 0.6, tail.y + Math.sin(ta) * tl * 0.6,
        tail.x + Math.cos(ta - 0.35) * tl * 1.35, tail.y + Math.sin(ta - 0.35) * tl * 1.35
      );
      ctx.quadraticCurveTo(
        tail.x + Math.cos(ta - 0.9) * tl, tail.y + Math.sin(ta - 0.9) * tl,
        tail.x, tail.y
      );
      ctx.closePath();
      ctx.fill();
    }

    _drawHead(ctx) {
      const h = this.pts[0];
      const R = this.maxRadius;
      ctx.save();
      ctx.translate(h.x, h.y);
      ctx.rotate(h.a);

      ctx.fillStyle = this.hornColor;
      for (let side = -1; side <= 1; side += 2) {
        ctx.strokeStyle = hexToRgba(this.hornColor, 1);
        ctx.lineCap = 'round';
        ctx.lineWidth = R * 0.3;
        ctx.beginPath();
        ctx.moveTo(R * 0.2, side * R * 0.75);
        ctx.quadraticCurveTo(-R * 0.9, side * R * 1.5, -R * 1.7, side * R * 1.15);
        ctx.stroke();
        ctx.lineWidth = R * 0.16;
        ctx.beginPath();
        ctx.moveTo(-R * 1.05, side * R * 1.42);
        ctx.quadraticCurveTo(-R * 1.5, side * R * 1.32, -R * 1.75, side * R * 1.12);
        ctx.stroke();
      }

      ctx.fillStyle = hexToRgba(this.bodyColor, 1);
      ctx.beginPath();
      ctx.ellipse(R * 0.45, 0, R * 1.4, R * 0.98, 0, 0, Math.PI * 2);
      ctx.fill();

      const m = R * 2.6;
      ctx.beginPath();
      ctx.moveTo(R * 0.9, -R * 0.55);
      ctx.quadraticCurveTo(m * 0.85, -R * 0.5, m, -R * 0.22);
      ctx.lineTo(m, R * 0.22);
      ctx.quadraticCurveTo(m * 0.85, R * 0.5, R * 0.9, R * 0.55);
      ctx.closePath();
      ctx.fill();

      if (this.jaw > 0.02) {
        ctx.fillStyle = '#1a0505';
        ctx.beginPath();
        ctx.moveTo(R * 1.0, -R * 0.1);
        ctx.lineTo(m, -R * 0.34 * this.jaw);
        ctx.lineTo(m, R * 0.34 * this.jaw);
        ctx.lineTo(R * 1.0, R * 0.1);
        ctx.closePath();
        ctx.fill();
      }

      ctx.fillStyle = '#2a0a0a';
      ctx.beginPath();
      ctx.arc(m - R * 0.12, -R * 0.15, R * 0.075, 0, Math.PI * 2);
      ctx.arc(m - R * 0.12, R * 0.15, R * 0.075, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = hexToRgba(this.eyeColor, 1);
      ctx.shadowColor = this.eyeColor;
      ctx.shadowBlur = R * 0.8;
      ctx.beginPath();
      ctx.arc(R * 0.5, -R * 0.72, R * 0.16, 0, Math.PI * 2);
      ctx.arc(R * 0.5, R * 0.72, R * 0.16, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;

      ctx.restore();
    }
  }

  FireDragon.version = '1.0.0';
  FireDragon.defaults = {
    scale: 1, radius: 14, spacing: null, segments: 32, speed: 300,
    turnRate: 3.2, flapSpeed: 6.5, wiggleFreq: 3.2, wiggleAmp: 10,
    bodyColor: '#b3202c', spineColor: '#7a1220', hornColor: '#e8c76b',
    eyeColor: '#ffd34d', fireHue: 45, autoFlight: false, controlWindow: true
  };

  global.FireDragon = FireDragon;
  if (typeof module !== 'undefined' && module.exports) module.exports = FireDragon;
})(typeof window !== 'undefined' ? window : this);
