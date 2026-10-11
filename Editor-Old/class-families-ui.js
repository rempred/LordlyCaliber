window.OB64 = window.OB64 || {};
(function(O) {
  'use strict';
  var managerOpen=false;
  function element(tag,text) { var e=document.createElement(tag);if(text)e.textContent=text;return e; }
  function option(select,value,label) { var e=element('option',label);e.value=String(value);select.appendChild(e); }
  function enabled(rom) { return O.buildModes.get(rom)==='rebuild'; }
  function familySelect(rom,classId,onChange) {
    var state=O.classFamilies.ensure(rom);
    if(!state||classId<1||classId>=state.assignments.length){var unavailable=element('span','—');unavailable.title='This class has no rebuilt manual-promotion entry yet.';return unavailable;}
    var select=element('select');select.setAttribute('aria-label','Family for '+O.className(classId));
    state.families.forEach(function(f){option(select,f.id,f.name+' ['+f.id+']');});
    select.value=String(state.assignments[classId]);select.disabled=!enabled(rom);
    select.addEventListener('change',function(){O.classFamilies.assign(rom,classId,Number(select.value));onChange();});
    return select;
  }
  function mount(rom,parent,onChange) {
    var state=O.classFamilies.ensure(rom);if(!state)return;
    var group=element('details');group.className='class-section';
    group.open=managerOpen;group.addEventListener('toggle',function(){managerOpen=group.open;});
    group.appendChild(element('summary','Class families'));
    group.appendChild(element('p','Choose families in the Promotion table or a class card. Create a family by copying an existing family’s promotion behavior. Existing category and special-class rules still apply; stat and alignment requirements remain editable separately.'));
    if(!enabled(rom))group.appendChild(element('p','Select Rebuild ROM to edit families.'));
    var label=element('label','New family name '),name=element('input');name.type='text';name.maxLength=80;
    name.setAttribute('aria-label','New family name');label.appendChild(name);group.appendChild(label);
    var baseLabel=element('label',' Copy behavior from '),base=element('select');base.setAttribute('aria-label','Family behavior template');
    state.families.forEach(function(f){
      if(f.id===0)return;
      var samples=[];state.assignments.forEach(function(id,index){if(id===f.id&&index&&samples.length<3)samples.push(O.className(index));});
      option(base,f.id,f.name+(samples.length?' — '+samples.join(', '):'')+
        ([1,2,5].indexOf(f.behavior)===-1?' (manual promotion disabled)':''));
    });
    base.value='1';baseLabel.appendChild(base);group.appendChild(baseLabel);
    var add=element('button','New Family'),message=element('p');message.setAttribute('role','status');
    name.disabled=base.disabled=add.disabled=!enabled(rom);
    add.addEventListener('click',function(){
      try { O.classFamilies.add(rom,name.value,Number(base.value));onChange(); }
      catch(error){message.textContent=error.message;}
    });
    group.appendChild(add);group.appendChild(message);parent.appendChild(group);
  }
  O.classFamiliesUI={mount:mount,select:familySelect};
})(window.OB64);
