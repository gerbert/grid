module.exports = [
  {
    type: 'heading',
    defaultValue: '//GRID Settings'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Display'
      },
      {
        type: 'slider',
        messageKey: 'GLANCE_DURATION_SEC',
        label: 'Glance duration',
        description: 'Seconds shown after the animation',
        defaultValue: 7,
        min: 3,
        max: 30,
        step: 1
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save Settings'
  }
];