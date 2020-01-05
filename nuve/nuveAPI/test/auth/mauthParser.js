/* global require, describe, it */


const mauthParser = require('../../auth/mauthParser');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('MAuth Parser', () => {
  const testParams = [
    { header: 'MAuth realm="arbitraryRealm"',
      json: { realm: '"arbitraryRealm"' } },
    { header: 'MAuth realm="arbitraryRealm",mauth_field="arbitraryField"',
      json: { realm: '"arbitraryRealm"',
        field: '"arbitraryField"' } },
    { header: 'MAuth realm="arbitraryRealm",mauth_username=arbitraryUser',
      json: { realm: '"arbitraryRealm"',
        username: 'arbitraryUser' } },
    { header: 'MAuth realm="arbitraryRealm",mauth_username=arbitraryUser,' +
             'mauth_field=arbitraryField',
    json: { realm: '"arbitraryRealm"',
      username: 'arbitraryUser',
      field: 'arbitraryField' } },
  ];

  describe('parseHeader', () => {
    testParams.forEach((param) => {
      it(`should parse ${param.header} to json ${JSON.stringify(param.json)}`, () => {
        const json = mauthParser.parseHeader(param.header);

        expect(json).to.deep.equal(param.json);
      });
    });
  });

  describe('makeHeader', () => {
    testParams.forEach((param) => {
      it(`should parse json ${JSON.stringify(param.json)} to ${param.header}`, () => {
        const header = mauthParser.makeHeader(param.json);

        expect(header).to.deep.equal(param.header);
      });
    });
  });

  describe('calculateClientSignature', () => {
    const clientSignatureParams = [
      {
        input: { timestamp: 1234, cnonce: 1 },
        key: '1234',
        signature: 'YzRjMGNjNjBiMzY3ZDY0NzY5YTk1OGYyMGIwYzIxM2Q3NWE0MWM4MA==',
      },
      {
        input: { timestamp: 4321, cnonce: 1 },
        key: '1234',
        signature: 'NTZmZTYzN2Q5Y2U4NWYyZDQzM2E1ZTg4NmUzNzlmOTI1ZjQ2N2NlNw==',
      },
      {
        input: { timestamp: 1234, cnonce: 1 },
        key: '4321',
        signature: 'MDRjZTc2MDFmNDlkZWM1MzUxYWE1ZDNmMmY1MWFhODk5ZmRlZDI1Mg==',
      },
      {
        input: { timestamp: 1234, cnonce: 2 },
        key: '1234',
        signature: 'YTJkOGJiNzhlYTdkZmUwOTljYzBlZDgxZGNkYWQyNDliYTUyM2M4Zg==',
      },
      {
        input: { timestamp: 1234, cnonce: 2, username: 'user' },
        key: '1234',
        signature: 'YTJkOGJiNzhlYTdkZmUwOTljYzBlZDgxZGNkYWQyNDliYTUyM2M4Zg==',
      },
      {
        input: { timestamp: 1234, cnonce: 2, role: 'role' },
        key: '1234',
        signature: 'YTJkOGJiNzhlYTdkZmUwOTljYzBlZDgxZGNkYWQyNDliYTUyM2M4Zg==',
      },
      {
        input: { timestamp: 1234, cnonce: 1, username: 'user', role: 'role' },
        key: '1234',
        signature: 'ZDE0Nzc1OTZhOTdhYWUyYzAzMzdjYjk4NGFhYWVlMjhjMWYxYmI1Yg==',
      }];
    clientSignatureParams.forEach((param) => {
      it(`should create a valid signature from ${JSON.stringify(param.input)}`, () => {
        const signature = mauthParser.calculateClientSignature(param.input, param.key);
        expect(signature).to.deep.equal(param.signature);
      });
    });
  });

  describe('calculateServerSignature', () => {
    const serverSignatureParams = [
      {
        timestamp: '1234',
        key: '1234',
        signature: 'ZGQ3MjgyZjllMTg2YTBhMjEzZjdkNTA2ZmNiZjY1MDM4ZGVkMmIyNA==',
      },
      {
        timestamp: '4321',
        key: '1234',
        signature: 'MTBiZjA5NTQwMmFhYzQ4Y2UwYmY0MTBiY2JmZjczMGYxY2U1Mjg4Nw==',
      },
      {
        timestamp: '1234',
        key: '4321',
        signature: 'MmNlYzRmYTU2ODE1MzcyZGJiNWQ0ODczOTg5NGE0ZWY1N2Y2MTNhMA==',
      }];
    serverSignatureParams.forEach((param) => {
      it(`should create a valid signature from timestamp ${
        JSON.stringify(param.timestamp)}`, () => {
        const signature = mauthParser.calculateServerSignature(param, param.key);
        expect(signature).to.deep.equal(param.signature);
      });
    });
  });
});
