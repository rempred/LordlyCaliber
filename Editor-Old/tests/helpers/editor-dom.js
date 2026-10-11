'use strict';
// Deterministic DOM/event/animation-frame substitute; this does not render a browser.
function installDom() {
  const scheduled = new Map(); let next = 0;
  const context = new Proxy({ createImageData: (w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }),
    getImageData: (x, y, w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }), measureText: () => ({ width: 20 }) },
    { get: (object, key) => key in object ? object[key] : () => {} });
  class Element {
    constructor(tag) { this.tagName = tag.toUpperCase(); this.children = []; this.attrs = {}; this.listeners = {}; this.style = {}; this._text = ''; this.scrollTop = 0; this.scrollLeft = 0; this.tabIndex = ['BUTTON','INPUT','SELECT','TEXTAREA'].includes(this.tagName) ? 0 : -1; this._value = undefined; this._selected = undefined;
      this.classList = { add: (...names) => { this.className = [...new Set((this.className || '').split(' ').concat(names))].join(' '); }, remove: (...names) => { this.className = (this.className || '').split(' ').filter(name => !names.includes(name)).join(' '); }, contains: name => (this.className || '').split(' ').includes(name), toggle: (name, force) => { if (force === undefined) force = !this.classList.contains(name); this.classList[force ? 'add' : 'remove'](name); } };
    }
    get isConnected() { return this === document.body || !!this.parentNode?.isConnected; }
    get firstChild() { return this.children[0] || null; }
    get childNodes() { return this.children; }
    get options() { return this.children; }
    get selected() { return this._selected === undefined ? this.parentNode?._value === this.value || (!this.parentNode?._value && this.parentNode?.children[0] === this) : this._selected; }
    set selected(value) { this._selected = value; }
    get selectedOptions() { return this.children.filter(child => child.selected); }
    get value() { if (this.tagName === 'SELECT') return this._value === undefined ? this.selectedOptions[0]?.value || '' : this._value; return this._value === undefined ? '' : this._value; }
    set value(value) { this._value = String(value); if (this.tagName === 'SELECT') this.children.forEach(child => { child._selected = child.value === this._value; }); }
    get textContent() { return this._text + this.children.map(child => child.textContent || '').join(''); }
    set textContent(value) { this.innerHTML = ''; this._text = String(value); }
    set innerHTML(value) { this.children.forEach(child => { child.parentNode = null; }); this.children = []; this._text = ''; }
    appendChild(child) { this.children.push(child); child.parentNode = this; return child; }
    append(...children) { children.forEach(child => this.appendChild(typeof child === 'string' ? document.createTextNode(child) : child)); }
    prepend(child) { this.children.unshift(child); child.parentNode = this; }
    removeChild(child) { this.children.splice(this.children.indexOf(child), 1); child.parentNode = null; return child; }
    remove() { this.parentNode?.removeChild(this); }
    replaceChildren(...children) { this.innerHTML = ''; this.append(...children); }
    contains(child) { return child === this || this.children.some(row => row.contains?.(child)); }
    setAttribute(key, value) { this.attrs[key] = String(value); if (key === 'id') this.id = value; if (key === 'tabindex') this.tabIndex = Number(value); }
    getAttribute(key) { return key === 'class' ? this.className || null : this.attrs[key] ?? null; }
    removeAttribute(key) { delete this.attrs[key]; }
    addEventListener(name, callback) { (this.listeners[name] ||= []).push(callback); }
    removeEventListener(name, callback) { this.listeners[name] = (this.listeners[name] || []).filter(row => row !== callback); }
    dispatch(name, init = {}) { const event = Object.assign({ type: name, target: this, key: '', preventDefault() { this.defaultPrevented = true; }, stopPropagation() {} }, init); (this.listeners[name] || []).slice().forEach(callback => callback(event)); return event; }
    click() { if (!this.disabled) this.dispatch('click'); }
    focus() { document.activeElement = this; }
    select() { this.selectionStart = 0; this.selectionEnd = this.value.length; }
    setSelectionRange(start, end) { this.selectionStart = start; this.selectionEnd = end; }
    getContext() { return context; }
    getBoundingClientRect() { return { left: 0, top: 0, width: this.width || 100, height: this.height || 100 }; }
    setPointerCapture(id) { this.capture = id; }
    hasPointerCapture(id) { return this.capture === id; }
    releasePointerCapture() { this.capture = null; }
    scrollIntoView() {}
    matches(selector) {
      selector = selector.trim();
      if (selector.includes(':not([disabled])')) { if (this.disabled) return false; selector = selector.replace(':not([disabled])', ''); }
      const attr = [...selector.matchAll(/\[([^=\]]+)(?:="?([^"\]]+)"?)?\]/g)];
      if (attr.some(([,key,value]) => value === undefined ? this.getAttribute(key) === null && !(key === 'tabindex' && this.tabIndex >= 0) : this.getAttribute(key) !== value)) return false;
      selector = selector.replace(/\[[^\]]+\]/g, '');
      if (selector.startsWith('#')) return this.id === selector.slice(1);
      if (selector.startsWith('.')) return this.classList.contains(selector.slice(1));
      return !selector || selector === '*' || this.tagName === selector.toUpperCase();
    }
    querySelectorAll(selector) { return this.children.flatMap(child => [child, ...child.querySelectorAll('*')]).filter(child => selector.split(',').some(part => child.matches(part))); }
    querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
    closest(selector) { return this.matches(selector) ? this : this.parentNode?.closest(selector) || null; }
  }
  global.document = { body: null, activeElement: null, hidden: false, createElement: tag => new Element(tag), createTextNode: text => { const n = new Element('text'); n.textContent = text; return n; },
    listeners: {}, addEventListener(name, callback) { (this.listeners[name] ||= []).push(callback); }, removeEventListener(name, callback) { this.listeners[name] = (this.listeners[name] || []).filter(row => row !== callback); },
    querySelector: selector => document.body.querySelector(selector), querySelectorAll: selector => document.body.querySelectorAll(selector) };
  document.body = new Element('body'); document.activeElement = document.body;
  global.requestAnimationFrame = callback => { const id = ++next; scheduled.set(id, callback); return id; };
  global.cancelAnimationFrame = id => scheduled.delete(id);
  global.confirm = () => true; global.scrollTo = () => {}; global.innerWidth = 1200; global.innerHeight = 800;
  return { Element, scheduled, tick(time) { const rows = [...scheduled.values()]; scheduled.clear(); rows.forEach(callback => callback(time)); },
    findText(root, text) { return root.querySelectorAll('*').find(node => node.textContent === text); } };
}
module.exports = { installDom };
